#include "matoya.h"
#include "miniz.h"

struct MTY_Archive {
	mz_zip_archive *zip;
	MTY_List *files;
	MTY_ListNode *current_file;
};

MTY_Archive *MTY_ArchiveOpen(const void *data, size_t size)
{
	MTY_Archive *ctx = MTY_Alloc(1, sizeof(MTY_Archive));

	ctx->zip = MTY_Alloc(1, sizeof(mz_zip_archive));
	mz_zip_reader_init_mem(ctx->zip, data, size, 0);
	mz_uint num_files = mz_zip_reader_get_num_files(ctx->zip);

	ctx->files = MTY_ListCreate();
	for (mz_uint i = 0; i < num_files; i++) {
		mz_zip_archive_file_stat file_stat = {0};
		mz_zip_reader_file_stat(ctx->zip, i, &file_stat);

		MTY_ArchiveFile *file = MTY_Alloc(1, sizeof(MTY_ArchiveFile));

		file->path = MTY_Strdup(file_stat.m_filename);
		file->directory = mz_zip_reader_is_file_a_directory(ctx->zip, i);
		file->size = (size_t) file_stat.m_uncomp_size;

		MTY_ListAppend(ctx->files, file);
	}

	return ctx;
}

static void mty_archive_free_file(void *file)
{
	if (!file)
		return;

	MTY_ArchiveFile *ctx = file;

	if (ctx->data)
		MTY_Free(ctx->data);

	if (ctx->path)
		MTY_Free(ctx->path);
	
	MTY_Free(ctx);
}

void MTY_ArchiveDestroy(MTY_Archive **archive)
{
	if (!archive || !*archive)
		return;

	MTY_Archive *ctx = *archive;

	MTY_ListDestroy(&ctx->files, mty_archive_free_file);

	if (ctx->zip) {
		mz_zip_reader_end(ctx->zip);
		MTY_Free(ctx->zip);
	}

	MTY_Free(ctx);
	*archive = NULL;
}

bool MTY_ArchiveReadFile(MTY_Archive *ctx, MTY_ArchiveFile **file)
{
	if (*file == NULL) {
		ctx->current_file = MTY_ListGetFirst(ctx->files);

	} else if (ctx->current_file) {
		ctx->current_file = ctx->current_file->next;
	}

	if (!ctx->current_file)
		return false;

	*file = ctx->current_file ? ctx->current_file->value : NULL;

	return *file != NULL;
}

bool MTY_ArchiveUnpack(MTY_Archive *ctx, MTY_ArchiveFile *file)
{
	file->data = mz_zip_reader_extract_file_to_heap(ctx->zip, file->path, NULL, 0);

	return file->data != NULL;
}
