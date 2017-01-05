This is an userland-only exploit for the 3DS HTTP-sysmodule. The configuration downloaded from the server then allows you to bypass required-sysupdate errors. This applies to the following: NetUpdateSOAP, friends-server(nasc), and NNID. In other words, this allows you to access everything(as of March 22, 2016) that's known to throw sysupdate-required errors on outdated system-versions(the only known exception is browser-version-check since this can't target web-browser httpc currently).

This also results in sysupdates being blocked from downloading normally(the system handles it the same way as if no sysupdate is available).

This can additionally be used for other things as well via the user_config optionally loaded from SD.

Once run successfully, ctr-httpwn will persist under the sysmodule until the sysmodule is terminated(shutdown/reboot/FIRM-launch). For example, you can't use this on Old3DS for Super Smash Bros / Monster Hunter, due to FIRM-launch.

The inital exploitation method was theorized in late 2015. The initial exploit was implemented on February 12-13, 2016.

The server config is downloaded with HTTPS from the yls8.mtheall.com site, likewise with the new_url for NetUpdateSOAP.

# Safety

A lot of effort went into avoiding sending *absolutely* *any* console-unique/account-unique data to any non-Nintendo server(s). With the default server config, the only requests that get redirected to a non-Nintendo server is NetUpdateSOAP. With NetUpdateSOAP the POST content-body is not uploaded at all by default, hence nothing that's really unique gets uploaded.

Please do not run any ctr-httpwn builds that are not from: the release-archives directly from this github repo, or the homebrew starter-kit(besides building it *yourself* from git). This also means any homebrew app downloaders which include ctr-httpwn should only download from the github release-achives here, no mirrors. *Please* *do* *not* distribute user_config which has new_url entries for NNID or any SOAP that's not NetUpdateSOAP. Be *extremely* *careful* with user_config / "user_nim_rootcertchain_rootca.der" files that aren't your own. You should verify all user_config before using it when you didn't write it yourself to make sure it doesn't have any of the previously mentioned new_url entries.

If this starts loading user_config unexpectedly when you didn't write anything to user_config, you should immediately power-off then check the contents of user_config.

Hashes for the release builds are available at "web/hashes" and here(https://yls8.mtheall.com/ctr-httpwn/hashes), the github+yls8.mtheall.com "hashes" files *must* match.

NOTE: ctr-httpwn v1.0.1 can't affect/change any network requests for adding eShop funds.

# Usage

Do not use this from Old3DS-browserhax. Do not run this app more than once when ctr-httpwn is already active under the sysmodule.

Just run the app, then if successful return to hbmenu. For using the system eShop application on <10.0.0-X, you must use HANS-eShop included with the homebrew starter-kit(this should be the one updated on March 20, 2016, or later). For everything else, and for using system-eShop-app on >=v10.0.0-X, you can just return to Home Menu from hbmenu. This can be done by pressing the START button, then use the option for returning to Home Menu without rebooting(you can use HANS if you *really* want to, except for eShop on >=10.0.0-X, returning to Home Menu is not *required*).

Returning to Home Menu from hbmenu without rebooting requires a version of hbmenu which actually supports it, and at least \*hax payload 2.6.

When using HANS-eShop on a *very* old system-version such as 9.6.0-X, eShop-app may display an error the first time. Returning to hbmenu then running HANS-eShop again fixes this issue caused(?) by friends-service. You'll have to repeat this every time you boot into \*hax payloads for using HANS-eShop.

In some rare(?) cases, during eShop startup it may unexpectedly start some sort of NNID-related(?) setup(unknown, NNID was mentioned in the Japanese text on a JPN New3DS at least) even though a NNID has been linked a long while. There's no known way to definitely reproduce this. There doesn't appear to be any other affects(eShop/elsewhere) once fully loaded into eShop however.

# Future sysupdates
When new sysupdates are released, the yls8.mtheall.com site will automatically determine all of the new version values to use with network services. This may take a little while after sysupdate release to finish, hence the server will return error messages until it's done(which is at the same time that the updatedetails text for the ninupdates report becomes available). When that happens you can use the cached server_config.xml, but that's really only useful if you want to use network services which don't require the new version values immediately(which is mainly/only(?) the nasc friends-server, also used with online play, mainly when friends-server wasn't accessible on your system pre-sysupdate to begin with).

Network-request changes(more than just version values) with network services in the future may cause using those network services with ctr-httpwn to break. This can't be handled automatically at sysupdate release. Depending on what changes, a ctr-httpwn update for those future changes may be required, if just updating the xml isn't enough.

# Supported sysmodule versions

Currently this is hard-coded for HTTP-sysmodule v13318. This is the latest sysmodule version as of 10.7.0-X, last updated with 9.6.0-X. It's unknown if/when auto-locating for the required sysmodule addresses would be implemented for supporting more versions.

__Using the system eShop application with EUR <v10.4 is broken(the application crashes during startup).__

# SD data

Config data is stored under the same directory as the .3dsx:
* "server_config.xml": The config downloaded from the server is written to here every time the config is successfully downloaded. If downloading config from the server fails, ctr-httpwn allows using this cached config if you want. ctr-httpwn checks for the existence of this file to determine whether to display the {read the documentation} message at startup.
* "user_config/": This directory contains the optional user_config .xml files. These are parsed the exact same way as the server config, however note that the "incompatsysver_message" element's text isn't displayed. The "user_config.xml" file from the same directory as the .3dsx is automatically moved into this directory("user_config.xml" was the fixed filepath for user_config pre-v1.2 ctr-httpwn).
* "user_nim_rootcertchain_rootca.der": When this exists, this cert is used for adding to the NIM RootCertChains instead of the built-in ctr-httpwn cert. Do not use this unless you changed the NetUpdateSOAP new_url or disabled the NetUpdateSOAP targeturl, via user_config.
* "url_config.txt" URL to use for downloading the server config from, instead of the default. The rootCA cert used for this is the ctr-httpwn builtin cert(Let's Encrypt) - however plain http is can be used too.

# Exploit details

See the source code regarding initial exploitation. ctr-httpwn under the sysmodule is all ROP. During setup this basically hooks httpc_CreateContext for all of the current main-service-sessions. From CreateContext it checks if the command input URL matches any targeturls' url. If so the URL is then overwritten with the new_url if set, and a vtable used with the created context is overwritten with the custom vtable for this targeturl. This context vtable is then used for hooking/overriding the vtable funcptrs used with httpc service commands specified in the targeturl caps.

This can only target httpc main-service-sessions which are open at the time this app runs. It's unknown if/when a way to target main-service-sessions opened after this app would be implemented, but right now it's not needed in the server config anyway. In other words, *only* the sysmodules using httpc can be targeted currently:

* boss(SpotPass): You could have SpotPass use network requests with your own URLs for any content you want, including the policylist file used internally(https://3dbrew.org/wiki/SpotPass). For content using the BOSS-container using your own URLs with that content is really only useful for returning archived SpotPass content(without other haxx allowing modifying that RSA-signed BOSS-container content).
* friends: This just uses the "/ac" page also targeted by the config returned by the server(for replacing the uploaded version values). This is used for friends-login/online-play, etc.
* act(NNID): This does all of the NNID-related network requests. The server config replaces all of the version values sent in these NNID requests with the values from the latest sysupdate.
* ac: Not really useful. This does the connection-test, but there's really nothing else used(this has code for using the nasc server but there's no known way to trigger that).
* NIM: Used for sysupdates and system/eShop title downloading. Also does eShop account network requests via SOAP. NetUpdateSOAP is targeted by the server config. With your own config which specifies a different new_url for NetUpdateSOAP, you could use network sysupdate installation to install any system titles you want that are *newer* than the currently installed ones(see web/NetUpdateSOAP.php). Be very careful with that, your system may brick if you install system titles(NATIVE_FIRM mainly) from the wrong model(Old3DS/New3DS). You could also install old titles from eShop with this when the title isn't installed currently, even with downloading all required files from your own server if needed.

# Configuration

The internal config is parsed first, then server xml, then lastly the user_config if it exists. See also the SD-data section above. For details on the configuration handling/format, see "configdoc.xml", "web/config.php", and the source code.

If the total size for all of the configuration in memory is too large, ROP size errors will be thrown(since that config has to be stored in sysmodule memory).

This basically only supports overwriting request data, not adding anything currently. This also means you can't add your own TLS certs with this currently(minus the NIM RootCertChains, doing so with the ACT RootCertChain isn't enabled for safety).

# Credits
* This uses the decompression code from here for ExeFS .code decompression: https://github.com/smealum/ninjhax2.x/blob/master/app_bootloader/source/takeover.c
* Tinyxml2 is used for config XML parsing, via portlibs.
* @ihaveamac for the app icon(issue #1).
* types.h at ipctakeover/boss/ is from ctrtool.
* The filepath for "url_config.txt" is from here: https://github.com/skiptirengu/ctr-httpwn

