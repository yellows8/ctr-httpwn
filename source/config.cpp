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

targeturlctx *config_findurltarget_entry(targeturlctx **first_targeturlctx, targeturlctx **prev_targeturlctx, char *name)
{
	targeturlctx *cur_targeturlctx;

	cur_targeturlctx = *first_targeturlctx;
	if(prev_targeturlctx)*prev_targeturlctx = NULL;

	while(cur_targeturlctx)
	{
		if(strncmp(name, cur_targeturlctx->name, sizeof(cur_targeturlctx->name)-1)==0)return cur_targeturlctx;

		if(prev_targeturlctx)*prev_targeturlctx = cur_targeturlctx;
		cur_targeturlctx = cur_targeturlctx->next;
	}

	return NULL;
}

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

int config_parse(configctx *config, char *xml)
{
	int ret=0;
	int resetflag;

	targeturlctx *cur_targeturlctx, **next_targeturlctx = NULL, *prev_targeturlctx = NULL, *tmp_targeturlctx;

	targeturl_requestoverridectx *cur_reqoverridectx, *tmp_reqoverridectx, **next_reqoverridectx_reqheader, **next_reqoverridectx_postform;

	XMLDocument doc;
	XMLElement *xml_targeturl = NULL;
	XMLElement *xml_reqoverride = NULL;
	XMLElement *xml_tmpelem;
	const char *textptr = NULL, *textptr2 = NULL;

	u32 pos, len;
	unsigned int tmpval=0;

	doc.Parse(xml);

	if(doc.Error())
	{
		printf("Failed to parse the xml under config_parse(): ");
		doc.PrintError();
		doc.Clear();
		return -1;
	}

	xml_tmpelem = doc.RootElement()->FirstChildElement("message");
	if(xml_tmpelem)
	{
		textptr = xml_tmpelem->GetText();
		if(textptr)
		{
			strncpy(config->message, textptr, sizeof(config->message)-1);

			config->message_prompt = 0;
			xml_tmpelem->QueryIntAttribute("prompt", &config->message_prompt);
		}
	}

	xml_tmpelem = doc.RootElement()->FirstChildElement("incompatsysver_message");
	if(xml_tmpelem)
	{
		textptr = xml_tmpelem->GetText();
		if(textptr)
		{
			strncpy(config->incompatsysver_message, textptr, sizeof(config->incompatsysver_message)-1);
		}
	}

	xml_targeturl = doc.RootElement()->FirstChildElement("targeturl");

	if(xml_targeturl)
	{
		for(; xml_targeturl; xml_targeturl = xml_targeturl->NextSiblingElement("targeturl"))
		{
			XMLElement *xml_caps = xml_targeturl->FirstChildElement("caps");
			XMLElement *xml_url = xml_targeturl->FirstChildElement("url");
			XMLElement *xml_new_url = xml_targeturl->FirstChildElement("new_url");

			tmp_targeturlctx = NULL;

			xml_tmpelem = xml_targeturl->FirstChildElement("name");
			if(xml_tmpelem)
			{
				textptr = xml_tmpelem->GetText();
				if(textptr)
				{
					if(textptr[0])
					{
						tmp_targeturlctx = config_findurltarget_entry(config->first_targeturlctx, &prev_targeturlctx, (char*)textptr);
					}
				}
			}

			if(tmp_targeturlctx)
			{
				cur_targeturlctx = tmp_targeturlctx;

				resetflag = 0;
				xml_targeturl->QueryIntAttribute("disabled", &resetflag);
				if(resetflag)
				{
					if(prev_targeturlctx)
					{
						prev_targeturlctx->next = cur_targeturlctx->next;
					}
					else
					{
						*config->first_targeturlctx = cur_targeturlctx->next;
					}

					config_freemem_reqoverride(&cur_targeturlctx->reqheader);
					config_freemem_reqoverride(&cur_targeturlctx->postform);

					free(cur_targeturlctx);
					continue;
				}
			}
			else
			{
				cur_targeturlctx = (targeturlctx*)malloc(sizeof(targeturlctx));
				if(cur_targeturlctx==NULL)
				{
					printf("config_parse(): Failed to allocate memory for cur_targeturlctx.\n");
					ret = -2;
					break;
				}
				memset(cur_targeturlctx, 0, sizeof(targeturlctx));

				if(*config->first_targeturlctx == NULL)
				{
					*config->first_targeturlctx = cur_targeturlctx;
				}
				else if(next_targeturlctx==NULL)
				{
					tmp_targeturlctx = *config->first_targeturlctx;
					while(tmp_targeturlctx)
					{
						next_targeturlctx = &tmp_targeturlctx->next;
						tmp_targeturlctx = *next_targeturlctx;
					}
				}

				if(next_targeturlctx)*next_targeturlctx = cur_targeturlctx;
				next_targeturlctx = &cur_targeturlctx->next;

				if(textptr)
				{
					if(textptr[0])strncpy(cur_targeturlctx->name, textptr, sizeof(cur_targeturlctx->name)-1);
				}
			}

			if(xml_caps)
			{
				resetflag = 0;
				xml_caps->QueryIntAttribute("reset", &resetflag);
				if(resetflag)cur_targeturlctx->caps = TARGETURLCAP_NONE;

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
				resetflag = 0;
				xml_url->QueryIntAttribute("reset", &resetflag);
				if(resetflag)memset(cur_targeturlctx->url, 0, sizeof(cur_targeturlctx->url));

				textptr = xml_url->GetText();

				if(textptr)strncpy(cur_targeturlctx->url, textptr, sizeof(cur_targeturlctx->url)-1);
			}

			if(xml_new_url)
			{
				resetflag = 0;
				xml_new_url->QueryIntAttribute("reset", &resetflag);
				if(resetflag)memset(cur_targeturlctx->new_url, 0, sizeof(cur_targeturlctx->new_url));

				textptr = xml_new_url->GetText();
				if(textptr)strncpy(cur_targeturlctx->new_url, textptr, sizeof(cur_targeturlctx->new_url)-1);
			}

			next_reqoverridectx_reqheader = &cur_targeturlctx->reqheader;
			next_reqoverridectx_postform = &cur_targeturlctx->postform;

			tmp_reqoverridectx = *next_reqoverridectx_reqheader;
			while(tmp_reqoverridectx)
			{
				next_reqoverridectx_reqheader = &tmp_reqoverridectx->next;
				tmp_reqoverridectx = *next_reqoverridectx_reqheader;
			}

			tmp_reqoverridectx = *next_reqoverridectx_postform;
			while(tmp_reqoverridectx)
			{
				next_reqoverridectx_postform = &tmp_reqoverridectx->next;
				tmp_reqoverridectx = *next_reqoverridectx_postform;
			}

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
					if(textptr)
					{
						textptr2 = xml_tmpelem->Attribute("format");

						if(textptr2==NULL || strcmp(textptr2, "hex"))
						{
							strncpy(cur_reqoverridectx->new_value, textptr, sizeof(cur_reqoverridectx->new_value)-1);
							len = strlen(cur_reqoverridectx->new_value);
						}
						else
						{
							len = strlen(textptr);
							len/=2;
							if(len>sizeof(cur_reqoverridectx->new_value))len=sizeof(cur_reqoverridectx->new_value);
							for(pos=0; pos<len; pos++)
							{
								tmpval = 0;
								sscanf(&textptr[pos*2], "%02x", &tmpval);
								cur_reqoverridectx->new_value[pos] = tmpval;
							}
						}

						cur_reqoverridectx->new_value_copysize = len;
					}
				}

				ret = config_parse_u32field(xml_reqoverride, "id", &cur_reqoverridectx->id);
				if(ret!=0)break;

				ret = config_parse_u32field(xml_reqoverride, "setid_onmatch", &cur_reqoverridectx->setid_onmatch);
				if(ret!=0)break;

				ret = config_parse_u32field(xml_reqoverride, "required_id", &cur_reqoverridectx->required_id);
				if(ret!=0)break;

				xml_tmpelem = xml_reqoverride->FirstChildElement("new_descriptorword_value");
				if(xml_tmpelem)
				{
					textptr = xml_tmpelem->GetText();
					if(textptr)
					{
						tmpval = 0;
						sscanf(textptr, "0x%x", &tmpval);
						cur_reqoverridectx->new_descriptorword_value = tmpval;
					}
				}

				ret = config_parse_u32field(xml_reqoverride, "enable_customcmdhandler", &cur_reqoverridectx->enable_customcmdhandler);
				if(ret!=0)break;

				xml_reqoverride = xml_reqoverride->NextSiblingElement("requestoverride");
			}

			if(ret!=0)break;
		}
	}

	doc.Clear();

	if(ret!=0)config_freemem(config);

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

void config_freemem(configctx *config)
{
	targeturlctx *cur_targeturlctx, *next_targeturlctx;

	cur_targeturlctx = *config->first_targeturlctx;
	*config->first_targeturlctx = NULL;

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

