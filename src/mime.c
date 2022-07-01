#include <string.h>

#include "matoya.h"

#define DEFAULT_TYPE "application/octet-stream"

bool initialized = false;
MTY_Hash *types = NULL;

static void mty_mime_initialize()
{
	if (initialized)
		return;

	types = MTY_HashCreate(0);

	// List retrieved from the MDN web docs:
	// https://developer.mozilla.org/fr/docs/Web/HTTP/Basics_of_HTTP/MIME_types/Common_types
	MTY_HashSet(types, ".aac",   "audio/aac");
	MTY_HashSet(types, ".abw",   "application/x-abiword");
	MTY_HashSet(types, ".arc",   "application/octet-stream");
	MTY_HashSet(types, ".avi",   "video/x-msvideo");
	MTY_HashSet(types, ".azw",   "application/vnd.amazon.ebook");
	MTY_HashSet(types, ".bin",   "application/octet-stream");
	MTY_HashSet(types, ".bmp",   "image/bmp");
	MTY_HashSet(types, ".bz",    "application/x-bzip");
	MTY_HashSet(types, ".bz2",   "application/x-bzip2");
	MTY_HashSet(types, ".csh",   "application/x-csh");
	MTY_HashSet(types, ".css",   "text/css");
	MTY_HashSet(types, ".csv",   "text/csv");
	MTY_HashSet(types, ".doc",   "application/msword");
	MTY_HashSet(types, ".docx",  "application/vnd.openxmlformats-officedocument.wordprocessingml.document");
	MTY_HashSet(types, ".eot",   "application/vnd.ms-fontobject");
	MTY_HashSet(types, ".epub",  "application/epub+zip");
	MTY_HashSet(types, ".gif",   "image/gif");
	MTY_HashSet(types, ".htm",   "text/html");
	MTY_HashSet(types, ".html",  "text/html");
	MTY_HashSet(types, ".ico",   "image/x-icon");
	MTY_HashSet(types, ".ics",   "text/calendar");
	MTY_HashSet(types, ".jar",   "application/java-archive");
	MTY_HashSet(types, ".jpeg",  "image/jpeg");
	MTY_HashSet(types, ".jpg",   "image/jpeg");
	MTY_HashSet(types, ".js",    "application/javascript");
	MTY_HashSet(types, ".json",  "application/json");
	MTY_HashSet(types, ".mid",   "audio/midi");
	MTY_HashSet(types, ".midi",  "audio/midi");
	MTY_HashSet(types, ".mpeg",  "video/mpeg");
	MTY_HashSet(types, ".mpkg",  "application/vnd.apple.installer+xml");
	MTY_HashSet(types, ".odp",   "application/vnd.oasis.opendocument.presentation");
	MTY_HashSet(types, ".ods",   "application/vnd.oasis.opendocument.spreadsheet");
	MTY_HashSet(types, ".odt",   "application/vnd.oasis.opendocument.text");
	MTY_HashSet(types, ".oga",   "audio/ogg");
	MTY_HashSet(types, ".ogv",   "video/ogg");
	MTY_HashSet(types, ".ogx",   "application/ogg");
	MTY_HashSet(types, ".otf",   "font/otf");
	MTY_HashSet(types, ".png",   "image/png");
	MTY_HashSet(types, ".pdf",   "application/pdf");
	MTY_HashSet(types, ".ppt",   "application/vnd.ms-powerpoint");
	MTY_HashSet(types, ".pptx",  "application/vnd.openxmlformats-officedocument.presentationml.presentation");
	MTY_HashSet(types, ".rar",   "application/x-rar-compressed");
	MTY_HashSet(types, ".rtf",   "application/rtf");
	MTY_HashSet(types, ".sh",    "application/x-sh");
	MTY_HashSet(types, ".svg",   "image/svg+xml");
	MTY_HashSet(types, ".swf",   "application/x-shockwave-flash");
	MTY_HashSet(types, ".tar",   "application/x-tar");
	MTY_HashSet(types, ".tif",   "image/tiff");
	MTY_HashSet(types, ".tiff",  "image/tiff");
	MTY_HashSet(types, ".ts",    "application/typescript");
	MTY_HashSet(types, ".ttf",   "font/ttf");
	MTY_HashSet(types, ".vsd",   "application/vnd.visio");
	MTY_HashSet(types, ".wav",   "audio/x-wav");
	MTY_HashSet(types, ".weba",  "audio/webm");
	MTY_HashSet(types, ".webm",  "video/webm");
	MTY_HashSet(types, ".webp",  "image/webp");
	MTY_HashSet(types, ".woff",  "font/woff");
	MTY_HashSet(types, ".woff2", "font/woff2");
	MTY_HashSet(types, ".xhtml", "application/xhtml+xml");
	MTY_HashSet(types, ".xls",   "application/vnd.ms-excel");
	MTY_HashSet(types, ".xlsx",  "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet");
	MTY_HashSet(types, ".xml",   "application/xml");
	MTY_HashSet(types, ".xul",   "application/vnd.mozilla.xul+xml");
	MTY_HashSet(types, ".zip",   "application/zip");
	MTY_HashSet(types, ".3gp",   "video/3gpp");
	MTY_HashSet(types, ".3g2",   "video/3gpp2");
	MTY_HashSet(types, ".7z",    "application/x-7z-compressed");

	initialized = true;
}

const char *MTY_MIMEGetType(const char *path)
{
	mty_mime_initialize();

	const char *extension = strrchr(path, '.');
	if (!extension)
		return DEFAULT_TYPE;

	const char *type = MTY_HashGet(types, extension);
	if (!type)
		return DEFAULT_TYPE;

	return type;
}
