#ifndef _NETWORK_EXPORTS_H_
#define _NETWORK_EXPORTS_H_

#if defined(_WIN32)

#if defined(mysql_chassis_proxy_EXPORTS)
#define NETWORK_API __declspec(dllexport)
#else
#define NETWORK_API extern __declspec(dllimport)
#endif

#else

#define NETWORK_API		extern

#endif

#endif

