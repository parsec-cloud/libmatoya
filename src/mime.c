#include <string.h>

#include "matoya.h"

#define DEFAULT_TYPE "application/octet-stream"

struct MTY_MIME {
	MTY_Hash *types;
};

MTY_MIME *MTY_MIMECreate()
{
	MTY_MIME *ctx = MTY_Alloc(1, sizeof(MTY_MIME));

	ctx->types = MTY_HashCreate(0);

	// List retrieved from the MDN web docs:
	// https://developer.mozilla.org/fr/docs/Web/HTTP/Basics_of_HTTP/MIME_types/Common_types
	MTY_HashSet(ctx->types, ".aac",   "audio/aac");
	MTY_HashSet(ctx->types, ".abw",   "application/x-abiword");
	MTY_HashSet(ctx->types, ".arc",   "application/octet-stream");
	MTY_HashSet(ctx->types, ".avi",   "video/x-msvideo");
	MTY_HashSet(ctx->types, ".azw",   "application/vnd.amazon.ebook");
	MTY_HashSet(ctx->types, ".bin",   "application/octet-stream");
	MTY_HashSet(ctx->types, ".bmp",   "image/bmp");
	MTY_HashSet(ctx->types, ".bz",    "application/x-bzip");
	MTY_HashSet(ctx->types, ".bz2",   "application/x-bzip2");
	MTY_HashSet(ctx->types, ".csh",   "application/x-csh");
	MTY_HashSet(ctx->types, ".css",   "text/css");
	MTY_HashSet(ctx->types, ".csv",   "text/csv");
	MTY_HashSet(ctx->types, ".doc",   "application/msword");
	MTY_HashSet(ctx->types, ".docx",  "application/vnd.openxmlformats-officedocument.wordprocessingml.document");
	MTY_HashSet(ctx->types, ".eot",   "application/vnd.ms-fontobject");
	MTY_HashSet(ctx->types, ".epub",  "application/epub+zip");
	MTY_HashSet(ctx->types, ".gif",   "image/gif");
	MTY_HashSet(ctx->types, ".htm",   "text/html");
	MTY_HashSet(ctx->types, ".html",  "text/html");
	MTY_HashSet(ctx->types, ".ico",   "image/x-icon");
	MTY_HashSet(ctx->types, ".ics",   "text/calendar");
	MTY_HashSet(ctx->types, ".jar",   "application/java-archive");
	MTY_HashSet(ctx->types, ".jpeg",  "image/jpeg");
	MTY_HashSet(ctx->types, ".jpg",   "image/jpeg");
	MTY_HashSet(ctx->types, ".js",    "application/javascript");
	MTY_HashSet(ctx->types, ".json",  "application/json");
	MTY_HashSet(ctx->types, ".mid",   "audio/midi");
	MTY_HashSet(ctx->types, ".midi",  "audio/midi");
	MTY_HashSet(ctx->types, ".mpeg",  "video/mpeg");
	MTY_HashSet(ctx->types, ".mpkg",  "application/vnd.apple.installer+xml");
	MTY_HashSet(ctx->types, ".odp",   "application/vnd.oasis.opendocument.presentation");
	MTY_HashSet(ctx->types, ".ods",   "application/vnd.oasis.opendocument.spreadsheet");
	MTY_HashSet(ctx->types, ".odt",   "application/vnd.oasis.opendocument.text");
	MTY_HashSet(ctx->types, ".oga",   "audio/ogg");
	MTY_HashSet(ctx->types, ".ogv",   "video/ogg");
	MTY_HashSet(ctx->types, ".ogx",   "application/ogg");
	MTY_HashSet(ctx->types, ".otf",   "font/otf");
	MTY_HashSet(ctx->types, ".png",   "image/png");
	MTY_HashSet(ctx->types, ".pdf",   "application/pdf");
	MTY_HashSet(ctx->types, ".ppt",   "application/vnd.ms-powerpoint");
	MTY_HashSet(ctx->types, ".pptx",  "application/vnd.openxmlformats-officedocument.presentationml.presentation");
	MTY_HashSet(ctx->types, ".rar",   "application/x-rar-compressed");
	MTY_HashSet(ctx->types, ".rtf",   "application/rtf");
	MTY_HashSet(ctx->types, ".sh",    "application/x-sh");
	MTY_HashSet(ctx->types, ".svg",   "image/svg+xml");
	MTY_HashSet(ctx->types, ".swf",   "application/x-shockwave-flash");
	MTY_HashSet(ctx->types, ".tar",   "application/x-tar");
	MTY_HashSet(ctx->types, ".tif",   "image/tiff");
	MTY_HashSet(ctx->types, ".tiff",  "image/tiff");
	MTY_HashSet(ctx->types, ".ts",    "application/typescript");
	MTY_HashSet(ctx->types, ".ttf",   "font/ttf");
	MTY_HashSet(ctx->types, ".vsd",   "application/vnd.visio");
	MTY_HashSet(ctx->types, ".wav",   "audio/x-wav");
	MTY_HashSet(ctx->types, ".weba",  "audio/webm");
	MTY_HashSet(ctx->types, ".webm",  "video/webm");
	MTY_HashSet(ctx->types, ".webp",  "image/webp");
	MTY_HashSet(ctx->types, ".woff",  "font/woff");
	MTY_HashSet(ctx->types, ".woff2", "font/woff2");
	MTY_HashSet(ctx->types, ".xhtml", "application/xhtml+xml");
	MTY_HashSet(ctx->types, ".xls",   "application/vnd.ms-excel");
	MTY_HashSet(ctx->types, ".xlsx",  "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet");
	MTY_HashSet(ctx->types, ".xml",   "application/xml");
	MTY_HashSet(ctx->types, ".xul",   "application/vnd.mozilla.xul+xml");
	MTY_HashSet(ctx->types, ".zip",   "application/zip");
	MTY_HashSet(ctx->types, ".3gp",   "video/3gpp");
	MTY_HashSet(ctx->types, ".3g2",   "video/3gpp2");
	MTY_HashSet(ctx->types, ".7z",    "application/x-7z-compressed");

	return ctx;
}

void MTY_MIMEDestroy(MTY_MIME **mime)
{
	if (!mime || !*mime)
		return;

	MTY_MIME *ctx = *mime;

	MTY_HashDestroy(&ctx->types, NULL);

	MTY_Free(ctx);
	*mime = NULL;
}

const char *MTY_MIMEGetType(MTY_MIME *ctx, const char *path)
{
	const char *extension = strrchr(path, '.');
	if (!extension)
		return DEFAULT_TYPE;

	const char *type = MTY_HashGet(ctx->types, extension);
	if (!type)
		return DEFAULT_TYPE;

	return type;
}
