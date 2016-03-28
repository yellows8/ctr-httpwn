<?php

include_once("/home/yellows8/ninupdates/api.php");

function initialize()
{
	global $mysqldb, $hdrs, $soapreq, $fp, $system, $region, $sitecfg_workdir, $soapreq_data;

	$soapreq_data = file_get_contents("php://input");

	$hdrs = array('SOAPAction: "urn:nus.wsapi.broadon.com/GetSystemUpdate"', 'Content-Type: application/xml', 'Content-Size: '.strlen($soapreq_data));
}

function init_curl()
{
	global $curl_handle, $sitecfg_workdir, $error_FH;

	$error_FH = fopen("$sitecfg_workdir/debuglogs/ctr-httpwn_web_NetUpdateSOAP_error.log","w");
	$curl_handle = curl_init();
}

function close_curl()
{
	global $curl_handle, $error_FH;

	curl_close($curl_handle);
	fclose($error_FH);
}

function send_httprequest($url)
{
	global $mysqldb, $hdrs, $soapreq, $httpstat, $sitecfg_workdir, $soapreq_data, $curl_handle, $system, $error_FH;

	$query="SELECT clientcertfn, clientprivfn FROM ninupdates_consoles WHERE system='".$system."'";
	$result=mysqli_query($mysqldb, $query);
	$row = mysqli_fetch_row($result);
	$clientcertfn = $row[0];
	$clientprivfn = $row[1];

	curl_setopt($curl_handle, CURLOPT_VERBOSE, true);
	curl_setopt ($curl_handle, CURLOPT_STDERR, $error_FH );

	curl_setopt($curl_handle, CURLOPT_USERAGENT, "CTR EC 040600 Mar 14 2012 13:32:39");

	curl_setopt($curl_handle, CURLOPT_RETURNTRANSFER, true);

	curl_setopt($curl_handle, CURLOPT_HTTPHEADER, $hdrs);

	curl_setopt($curl_handle, CURLOPT_POST, 1);
	curl_setopt($curl_handle, CURLOPT_POSTFIELDS, $soapreq_data);

	curl_setopt($curl_handle, CURLOPT_URL, $url);

	if(strstr($url, "https") && $clientcertfn!="" && $clientprivfn!="")
	{
		curl_setopt($curl_handle, CURLOPT_SSLCERTTYPE, "PEM");
		curl_setopt($curl_handle, CURLOPT_SSLCERT, "$sitecfg_workdir/sslcerts/$clientcertfn");
		curl_setopt($curl_handle, CURLOPT_SSLKEY, "$sitecfg_workdir/sslcerts/$clientprivfn");

		curl_setopt($curl_handle, CURLOPT_SSL_VERIFYPEER, 0);
		curl_setopt($curl_handle, CURLOPT_SSL_VERIFYHOST, 0);
	}

	$buf = curl_exec($curl_handle);

	$errorstr = "";

	$httpstat = curl_getinfo($curl_handle, CURLINFO_HTTP_CODE);
	if($buf===FALSE)
	{
		$errorstr = "HTTP request failed: " . curl_error ($curl_handle);
		$httpstat = "0";
	} else if($httpstat!="200")$errorstr = "HTTP error $httpstat: " . curl_error ($curl_handle);

	if($errorstr!="")$buf = $errorstr;

	return $buf;
}

$SOAPAction = $_SERVER['HTTP_SOAPACTION'];
$SOAPAction_prefix = "urn:nus.wsapi.broadon.com/";

$reply_TitleHash = "00000000000000000000000000000000";

$soapdata_filepath = "$sitecfg_workdir/ctr-httpwn/NetUpdateSOAP_soapreplydata";
$soapdata = "";
if(file_exists($soapdata_filepath))$soapdata = file_get_contents($soapdata_filepath);

if($soapdata!=="")
{
	$titlehash_pos = strpos($soapdata, "<TitleHash>") + 11;
	$titlehash_posend = strpos($soapdata, "</TitleHash>");
	if($titlehash_pos!==FALSE && $titlehash_posend!==FALSE)
	{
		$reply_TitleHash = substr($soapdata, $titlehash_pos, $titlehash_posend - $titlehash_pos);
	}
}

if($SOAPAction === $SOAPAction_prefix . "GetSystemTitleHash")
{
	$GetSystemTitleHashResponse = "<?xml version=\"1.0\" encoding=\"utf-8\"?><soapenv:Envelope xmlns:soapenv=\"http://schemas.xmlsoap.org/soap/envelope/\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"><soapenv:Body><GetSystemTitleHashResponse xmlns=\"urn:nus.wsapi.broadon.com\"><Version>1.0</Version><DeviceId></DeviceId><MessageId></MessageId><TimeStamp></TimeStamp><ErrorCode>0</ErrorCode><TitleHash>$reply_TitleHash</TitleHash></GetSystemTitleHashResponse></soapenv:Body></soapenv:Envelope>";

	echo $GetSystemTitleHashResponse;
}
else if($SOAPAction === $SOAPAction_prefix . "GetSystemUpdate")
{
	$GetSystemUpdateResponse = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><soapenv:Envelope xmlns:soapenv=\"http://schemas.xmlsoap.org/soap/envelope/\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"><soapenv:Body><GetSystemUpdateResponse xmlns=\"urn:nus.wsapi.broadon.com\"><Version>1.0</Version><DeviceId></DeviceId><MessageId>1</MessageId><TimeStamp></TimeStamp><ErrorCode>0</ErrorCode><ContentPrefixURL>http://nus.cdn.c.shop.nintendowifi.net/ccs/download</ContentPrefixURL><UncachedContentPrefixURL>https://ccs.c.shop.nintendowifi.net/ccs/download</UncachedContentPrefixURL><TitleVersion><TitleId>0004003000008A02</TitleId><Version>0</Version><FsSize>262144</FsSize><TicketSize>848</TicketSize><TMDSize>4660</TMDSize></TitleVersion><UploadAuditData>1</UploadAuditData><TitleHash>$reply_TitleHash</TitleHash></GetSystemUpdateResponse></soapenv:Body></soapenv:Envelope>";

	if($soapdata!=="")$GetSystemUpdateResponse = $soapdata;

	echo $GetSystemUpdateResponse;
}
else if($SOAPAction === $SOAPAction_prefix . "GetSystemCommonETicket")
{
	$system = "ctr";

	dbconnection_start();

	db_checkmaintenance(1);

	init_curl();
	initialize();
	$con = send_httprequest("https://nus.c.shop.nintendowifi.net/nus/services/NetUpdateSOAP");
	close_curl();

	dbconnection_end();

	echo $con;
}
else
{
	echo "Unrecognized SOAPAction header, it probably wasn't set correctly.";
}

?>
