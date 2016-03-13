#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <3ds.h>

#include <tinyxml2.h>

#include "config.h"

using namespace tinyxml2;

int config_parse_u32field(XMLElement *input_elem, const char *name, u32 *out)
{
	XMLElement *xml_tmpelem;
	int tmpint=0;

	xml_tmpelem = input_elem->FirstChildElement(name);
	if(xml_tmpelem)
	{
		tmpint = 0;
		if(xml_tmpelem->QueryIntText(&tmpint) == XML_SUCCESS)
		{
			if(tmpint < 0)
			{
				printf("config_parse_u32field(): The '%s' value from a xml element is invalid.\n", name);
				return -4;
			}

			*out = (u32)tmpint;
		}
	}

	return 0;
}

int config_parse(targeturlctx **first_targeturlctx, char *xml)
{
	int ret=0;
	u32 entcount = 0;

	targeturlctx *cur_targeturlctx, **next_targeturlctx = NULL;

	targeturl_requestoverridectx *cur_reqoverridectx, **next_reqoverridectx_reqheader, **next_reqoverridectx_postform;

	XMLDocument doc;
	XMLElement *xml_targeturl = NULL;
	XMLElement *xml_reqoverride = NULL;
	XMLElement *xml_tmpelem;
	const char *textptr = NULL;

	doc.Parse(xml);

	if(doc.Error())
	{
		printf("Failed to parse the xml under config_parse(): ");
		doc.PrintError();
		doc.Clear();
		return -1;
	}

	xml_targeturl = doc.RootElement()->FirstChildElement("targeturl");

	while(xml_targeturl)
	{
		XMLElement *xml_caps = xml_targeturl->FirstChildElement("caps");
		XMLElement *xml_url = xml_targeturl->FirstChildElement("url");
		XMLElement *xml_new_url = xml_targeturl->FirstChildElement("new_url");

		cur_targeturlctx = (targeturlctx*)malloc(sizeof(targeturlctx));
		if(cur_targeturlctx==NULL)
		{
			printf("config_parse(): Failed to allocate memory for cur_targeturlctx.\n");
			ret = -2;
			break;
		}
		memset(cur_targeturlctx, 0, sizeof(targeturlctx));

		if(next_targeturlctx)*next_targeturlctx = cur_targeturlctx;
		next_targeturlctx = &cur_targeturlctx->next;

		if(entcount==0)*first_targeturlctx = cur_targeturlctx;
		entcount++;

		if(xml_caps)
		{
			textptr = xml_caps->GetText();
			if(textptr)
			{
				if(strstr(textptr, "AddRequestHeader"))cur_targeturlctx->caps = (targeturl_caps)(cur_targeturlctx->caps | TARGETURLCAP_AddRequestHeader);
				if(strstr(textptr, "AddPostDataAscii"))cur_targeturlctx->caps = (targeturl_caps)(cur_targeturlctx->caps | TARGETURLCAP_AddPostDataAscii);
				if(strstr(textptr, "SendPOSTDataRawTimeout"))cur_targeturlctx->caps = (targeturl_caps)(cur_targeturlctx->caps | TARGETURLCAP_SendPOSTDataRawTimeout);
			}
		}

		if(xml_url)
		{
			textptr = xml_url->GetText();
			if(textptr)strncpy(cur_targeturlctx->url, textptr, sizeof(cur_targeturlctx->url)-1);
		}

		if(xml_new_url)
		{
			textptr = xml_new_url->GetText();
			if(textptr)strncpy(cur_targeturlctx->new_url, textptr, sizeof(cur_targeturlctx->new_url)-1);
		}

		next_reqoverridectx_reqheader = &cur_targeturlctx->reqheader;
		next_reqoverridectx_postform = &cur_targeturlctx->postform;

		xml_reqoverride = xml_targeturl->FirstChildElement("requestoverride");
		while(xml_reqoverride)
		{
			textptr = xml_reqoverride->Attribute("type");
			if(textptr==NULL)
			{
				printf("config_parse(): Failed to load the type attribute from a requestoverride element.\n");
				ret = -2;
				break;
			}

			cur_reqoverridectx = (targeturl_requestoverridectx*)malloc(sizeof(targeturl_requestoverridectx));
			if(cur_reqoverridectx==NULL)
			{
				printf("config_parse(): Failed to allocate memory for cur_reqoverridectx.\n");
				ret = -2;
				break;
			}
			memset(cur_reqoverridectx, 0, sizeof(targeturl_requestoverridectx));

			if(strcmp(textptr, "reqheader")==0)
			{
				*next_reqoverridectx_reqheader = cur_reqoverridectx;
				next_reqoverridectx_reqheader = &cur_reqoverridectx->next;
			}
			else if(strcmp(textptr, "postform")==0)
			{
				*next_reqoverridectx_postform = cur_reqoverridectx;
				next_reqoverridectx_postform = &cur_reqoverridectx->next;
			}
			else
			{
				printf("config_parse(): The type attribute from a requestoverride element is invalid.\n");
				ret = -3;
				break;
			}

			xml_tmpelem = xml_reqoverride->FirstChildElement("name");
			if(xml_tmpelem)
			{
				textptr = xml_tmpelem->GetText();
				if(textptr)strncpy(cur_reqoverridectx->name, textptr, sizeof(cur_reqoverridectx->name)-1);
			}

			xml_tmpelem = xml_reqoverride->FirstChildElement("value");
			if(xml_tmpelem)
			{
				textptr = xml_tmpelem->GetText();
				if(textptr)strncpy(cur_reqoverridectx->value, textptr, sizeof(cur_reqoverridectx->value)-1);
			}

			xml_tmpelem = xml_reqoverride->FirstChildElement("new_value");
			if(xml_tmpelem)
			{
				textptr = xml_tmpelem->GetText();
				if(textptr)strncpy(cur_reqoverridectx->new_value, textptr, sizeof(cur_reqoverridectx->new_value)-1);
			}

			ret = config_parse_u32field(xml_reqoverride, "id", &cur_reqoverridectx->id);
			if(ret!=0)break;

			ret = config_parse_u32field(xml_reqoverride, "setid_onmatch", &cur_reqoverridectx->setid_onmatch);
			if(ret!=0)break;

			ret = config_parse_u32field(xml_reqoverride, "required_id", &cur_reqoverridectx->required_id);
			if(ret!=0)break;

			xml_reqoverride = xml_reqoverride->NextSiblingElement("requestoverride");
		}

		if(ret!=0)break;

		xml_targeturl = xml_targeturl->NextSiblingElement("targeturl");
	}

	doc.Clear();

	if(ret!=0)config_freemem(first_targeturlctx);

	return ret;
}

void config_freemem_reqoverride(targeturl_requestoverridectx **first_reqoverridectx)
{
	targeturl_requestoverridectx *cur_reqoverridectx, *next_reqoverridectx;

	cur_reqoverridectx = *first_reqoverridectx;
	*first_reqoverridectx = NULL;

	while(cur_reqoverridectx)
	{
		next_reqoverridectx = cur_reqoverridectx->next;

		memset(cur_reqoverridectx, 0, sizeof(targeturlctx));
		free(cur_reqoverridectx);

		cur_reqoverridectx = next_reqoverridectx;
	}
}

void config_freemem(targeturlctx **first_targeturlctx)
{
	targeturlctx *cur_targeturlctx, *next_targeturlctx;

	cur_targeturlctx = *first_targeturlctx;
	*first_targeturlctx = NULL;

	while(cur_targeturlctx)
	{
		config_freemem_reqoverride(&cur_targeturlctx->reqheader);
		config_freemem_reqoverride(&cur_targeturlctx->postform);

		next_targeturlctx = cur_targeturlctx->next;

		memset(cur_targeturlctx, 0, sizeof(targeturlctx));
		free(cur_targeturlctx);

		cur_targeturlctx = next_targeturlctx;
	}
}

