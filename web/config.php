<?php

include_once("/home/yellows8/ninupdates/api.php");

$retval = ninupdates_api("gettitleversions", "ctr", "E", "000400DB00016302", 2);
if($retval!=0)
{
	writeNormalLog("ctr-httpwn config.php: API returned error $retval, for NVer. RESULT: 500");
	header('HTTP/1.1 500 Internal Server Error');
	die("A server-side error occured.\n");
}

$NVer_hextitlever = sprintf("%04X", substr($ninupdatesapi_out_version_array[0], 1));

$retval = ninupdates_api("gettitleversions", "ctr", "E", "0004013000003202", 2);
if($retval!=0)
{
	writeNormalLog("ctr-httpwn config.php: API returned error $retval, for friends-module. RESULT: 500");
	header('HTTP/1.1 500 Internal Server Error');
	die("A server-side error occured.\n");
}

$fpdver = file_get_contents("$sitecfg_workdir/friendsmodule_fpdvers/" . $ninupdatesapi_out_version_array[0]);
if($fpdver===FALSE || $fpdver==="")
{
	writeNormalLog("ctr-httpwn config.php: Failed to load a valid fpdver file. RESULT: 500");
	header('HTTP/1.1 500 Internal Server Error');
	die("The config xml is currently not available, probably due to a new sysupdate just being released. It will automatically become available once ninupdates sysupdate post-processing finishes.\n");
}

//Base64-encode fpdver, with '=' switched to '*'.
$fpdverbase64 = base64_encode($fpdver);
while(strpos($fpdverbase64, "=")!==FALSE)$fpdverbase64 = str_replace("=", "*", $fpdverbase64);

$targets_array = array();
$targets_verarray = array();

$targets_array[] = "tiger,eShop app,0004001000020900,JPN,J";
$targets_array[] = "tiger,eShop app,0004001000021900,USA,E";
$targets_array[] = "tiger,eShop app,0004001000022900,EUR,P";
$targets_array[] = "mint,mint,000400300000C602,JPN,J";
$targets_array[] = "mint,mint,000400300000CE02,USA,E";
$targets_array[] = "mint,mint,000400300000D602,EUR,P";
$targets_array[] = "mset,System Settings,0004001000020000,JPN,J";
$targets_array[] = "mset,System Settings,0004001000021000,USA,E";
$targets_array[] = "mset,System Settings,0004001000022000,EUR,P";
$targets_array[] = "cardboar,System Transfer,0004001000020A00,JPN,J";
$targets_array[] = "cardboar,System Transfer,0004001000021A00,USA,E";
$targets_array[] = "cardboar,System Transfer,0004001000022A00,EUR,P";
$targets_array[] = "nnid_settings,NNID Settings,000400100002BF00,JPN,J";
$targets_array[] = "nnid_settings,NNID Settings,000400100002C000,USA,E";
$targets_array[] = "nnid_settings,NNID Settings,000400100002C100,EUR,P";

$targets_array_size = count($targets_array);

for($i=0; $i<$targets_array_size; $i++)
{
	$titlename = strtok($targets_array[$i], ",");
	$printname = strtok(",");
	$titleid = strtok(",");
	$region = strtok(",");
	$regid = strtok(",");

	$retval = ninupdates_api("gettitleversions", "ctr", $regid, $titleid, 2);
	if($retval!=0)
	{
		writeNormalLog("ctr-httpwn config.php: API returned error $retval, for target $i. RESULT: 500");
		header('HTTP/1.1 500 Internal Server Error');
		die("A server-side error occured.\n");
	}

	$remaster_version = file_get_contents("$sitecfg_workdir/remaster_versions/$titlename/$region/" . $ninupdatesapi_out_version_array[0]);
	if($remaster_version===FALSE || $remaster_version==="")
	{
		writeNormalLog("ctr-httpwn config.php: Failed to load a valid remaster_version file, for target $i. RESULT: 500");
		header('HTTP/1.1 500 Internal Server Error');
		die("The config xml is currently not available, probably due to a new sysupdate just being released. It will automatically become available once ninupdates sysupdate post-processing finishes.\n");
	}

	$targets_verarray[] = $remaster_version;
}

$reqoverrides = "";
$reqoverrides_baseid = 16;

for($i=0; $i<$targets_array_size; $i++)
{
	$titlename = strtok($targets_array[$i], ",");
	$printname = strtok(",");
	$titleid = strtok(",");
	$region = strtok(",");
	$regid = strtok(",");
	$curid = $reqoverrides_baseid + $i;

	$reqoverrides.="
	<!-- $region $printname titleID. -->
	<requestoverride type=\"reqheader\">
		<id>$curid</id>
		<setid_onmatch>1</setid_onmatch>
		<name>X-Nintendo-Title-ID</name>
		<value>$titleid</value>
	</requestoverride>\n";
}

for($i=0; $i<$targets_array_size; $i++)
{
	$titlename = strtok($targets_array[$i], ",");
	$printname = strtok(",");
	$titleid = strtok(",");
	$region = strtok(",");
	$regid = strtok(",");

	$reqid = $reqoverrides_baseid + $i;
	$curid = $reqid + $targets_array_size;

	$remaster_version = $targets_verarray[$i];

	$reqoverrides.="
	<!-- Remaster version override for $region $printname. -->
	<requestoverride type=\"reqheader\">
		<id>$curid</id>
		<required_id>$reqid</required_id>
		<name>X-Nintendo-Application-Version</name>
		<new_value>$remaster_version</new_value>
	</requestoverride>\n";
}
?>
<?xml version="1.0" encoding="UTF-8"?>
<config>

	<incompatsysver_message>The sysmodule version must be the one from system-version >=9.6.0-X.</incompatsysver_message>

	<targeturl> <!-- This is the URL used for doing the actual sysupdate check / getting the the list of sysupdate titles. -->
		<name>NetUpdateSOAP</name>
		<caps>SendPOSTDataRawTimeout</caps>
		<url>https://nus.c.shop.nintendowifi.net/nus/services/NetUpdateSOAP</url>
		<new_url>https://yls8.mtheall.com/ctr-httpwn/NetUpdateSOAP.php</new_url>
	</targeturl>

	<targeturl>
		<name>nasc</name>
		<caps>AddRequestHeader AddPostDataAscii</caps>
		<url>https://nasc.nintendowifi.net/ac</url>

		<requestoverride type="reqheader">
			<name>User-Agent</name>
			<new_value>CTR FPD/<?=$fpdver?></new_value>
		</requestoverride>

		<requestoverride type="postform">
			<name>fpdver</name>
			<new_value><?=$fpdverbase64?></new_value>
		</requestoverride>
		
		<requestoverride type="postform">
			<name>gamever</name>
			<new_value>RkZGRg**</new_value>
		</requestoverride>
	</targeturl>

	<targeturl> <!-- NNID -->
		<name>NNID</name>
		<caps>AddRequestHeader</caps>
		<url>https://account.nintendo.net/</url>

		<requestoverride type="reqheader">
			<id>3</id>
			<name>X-Nintendo-System-Version</name>
			<new_value><?=$NVer_hextitlever?></new_value>
		</requestoverride>

		<?=$reqoverrides?>
	</targeturl>
</config>

