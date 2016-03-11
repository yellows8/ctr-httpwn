<?php

$SOAPAction = $_SERVER['HTTP_SOAPACTION'];
$SOAPAction_prefix = "urn:nus.wsapi.broadon.com/";

$reply_TitleHash = "00000000000000000000000000000000";

if($SOAPAction === $SOAPAction_prefix . "GetSystemTitleHash")
{
	$GetSystemTitleHashResponse = "<?xml version=\"1.0\" encoding=\"utf-8\"?><soapenv:Envelope xmlns:soapenv=\"http://schemas.xmlsoap.org/soap/envelope/\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"><soapenv:Body><GetSystemTitleHashResponse xmlns=\"urn:nus.wsapi.broadon.com\"><Version>1.0</Version><DeviceId></DeviceId><MessageId></MessageId><TimeStamp></TimeStamp><ErrorCode>0</ErrorCode><TitleHash>$reply_TitleHash</TitleHash></GetSystemTitleHashResponse></soapenv:Body></soapenv:Envelope>";

	echo $GetSystemTitleHashResponse;
}
else if($SOAPAction === $SOAPAction_prefix . "GetSystemUpdate")
{
	$GetSystemUpdateResponse = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><soapenv:Envelope xmlns:soapenv=\"http://schemas.xmlsoap.org/soap/envelope/\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"><soapenv:Body><GetSystemUpdateResponse xmlns=\"urn:nus.wsapi.broadon.com\"><Version>1.0</Version><DeviceId></DeviceId><MessageId>1</MessageId><TimeStamp></TimeStamp><ErrorCode>0</ErrorCode><ContentPrefixURL>http://nus.cdn.c.shop.nintendowifi.net/ccs/download</ContentPrefixURL><UncachedContentPrefixURL>https://ccs.c.shop.nintendowifi.net/ccs/download</UncachedContentPrefixURL><TitleVersion><TitleId>0004003000008A02</TitleId><Version>0</Version><FsSize>262144</FsSize><TicketSize>848</TicketSize><TMDSize>4660</TMDSize></TitleVersion><UploadAuditData>1</UploadAuditData><TitleHash>$reply_TitleHash</TitleHash></GetSystemUpdateResponse></soapenv:Body></soapenv:Envelope>";

	echo $GetSystemUpdateResponse;
}
else if($SOAPAction === $SOAPAction_prefix . "GetSystemCommonETicket")
{
	echo "GetSystemCommonETicket is currently not supported.";
}
else
{
	echo "Unrecognized SOAPAction header, it probably wasn't set correctly.";
}

?>
