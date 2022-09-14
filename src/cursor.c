#include "matoya.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#define VALIDATE_CTX(ctx, ...) if (!ctx) return __VA_ARGS__

#define CURSOR_ID 0x23C84DF2

#define CMD_LEN 2
#define VTX_LEN 4
#define IDX_LEN 6

struct MTY_Cursor {
	MTY_App *app;
	MTY_Window window;

	bool enabled;
	void *image;
	bool image_changed;

	uint32_t width;
	uint32_t height;
	float x;
	float y;
	float hot_x;
	float hot_y;
	float scale;

	MTY_DrawData *draw_data;
	MTY_CmdList *cmd_list;
};

MTY_Cursor *MTY_CursorCreate(MTY_App *app, MTY_Window window)
{
	MTY_Cursor *ctx = MTY_Alloc(1, sizeof(MTY_Cursor));

	ctx->app = app;
	ctx->window = window;
	ctx->enabled = true;

	ctx->cmd_list = MTY_Alloc(1, sizeof(MTY_CmdList));

	MTY_Cmd commands[CMD_LEN] = {
		{
			.texture = CURSOR_ID,
			.elemCount = 3,
			.idxOffset = 0,
			.clip = {0, 0, 0, 0},
		},
		{
			.texture = CURSOR_ID,
			.elemCount = 3,
			.idxOffset = 3,
			.clip = {0, 0, 0, 0},
		},
	};

	MTY_Vtx vertices[VTX_LEN] = {
		{ {0, 0}, {0, 0}, 0xFFFFFFFF },
		{ {0, 0}, {1, 0}, 0xFFFFFFFF },
		{ {0, 0}, {1, 1}, 0xFFFFFFFF },
		{ {0, 0}, {0, 1}, 0xFFFFFFFF },
	};

	uint16_t indices[IDX_LEN] = { 0, 1, 2, 0, 2, 3 };

	*ctx->cmd_list = (MTY_CmdList) {
		.cmd = MTY_Alloc(CMD_LEN, sizeof(MTY_Cmd)),
		.cmdLength = CMD_LEN,
		.cmdMax = CMD_LEN,

		.vtx = MTY_Alloc(VTX_LEN, sizeof(MTY_Vtx)),
		.vtxLength = VTX_LEN,
		.vtxMax = VTX_LEN,

		.idx = MTY_Alloc(IDX_LEN, sizeof(uint16_t)),
		.idxLength = IDX_LEN,
		.idxMax = IDX_LEN,
	};

	memcpy(ctx->cmd_list->cmd, commands, sizeof(MTY_Cmd[CMD_LEN]));
	memcpy(ctx->cmd_list->vtx, vertices, sizeof(MTY_Vtx[VTX_LEN]));
	memcpy(ctx->cmd_list->idx, indices, sizeof(uint16_t[IDX_LEN]));

	return ctx;
}

void MTY_CursorDestroy(MTY_Cursor **cursor)
{
	if (!cursor || !*cursor)
		return;

	MTY_Cursor *ctx = *cursor;

	MTY_WindowSetUITexture(ctx->app, ctx->window, CURSOR_ID, NULL, 0, 0);

	if (ctx->draw_data) {
		MTY_Free(ctx->draw_data->cmdList);
		MTY_Free(ctx->draw_data);
	}

	MTY_Free(ctx->cmd_list->idx);
	MTY_Free(ctx->cmd_list->vtx);
	MTY_Free(ctx->cmd_list->cmd);
	MTY_Free(ctx->cmd_list);

	if (ctx->image)
		MTY_Free(ctx->image);

	MTY_Free(ctx);
	*cursor = NULL;
}

void MTY_CursorEnable(MTY_Cursor *ctx, bool enable)
{
	VALIDATE_CTX(ctx);

	ctx->enabled = enable;
}

void MTY_CursorSetHotspot(MTY_Cursor *ctx, uint16_t hotX, uint16_t hotY)
{
	VALIDATE_CTX(ctx);

	ctx->hot_x = (float) hotX;
	ctx->hot_y = (float) hotY;
}

void MTY_CursorSetImage(MTY_Cursor *ctx, const void *data, size_t size)
{
	VALIDATE_CTX(ctx);

	if (ctx->image)
		MTY_Free(ctx->image);

	ctx->image = MTY_DecompressImage(data, size, &ctx->width, &ctx->height);
	ctx->image_changed = true;
}

void MTY_CursorMove(MTY_Cursor *ctx, int32_t x, int32_t y, float scale)
{
	VALIDATE_CTX(ctx);

	ctx->scale = scale;
	ctx->x = (float) x;
	ctx->y = (float) y;
}

void MTY_CursorMoveFromZoom(MTY_Cursor *ctx, MTY_Zoom *zoom)
{
	VALIDATE_CTX(ctx);

	float scale = MTY_ZoomGetScale(zoom);
	int32_t cursor_x = MTY_ZoomGetCursorX(zoom);
	int32_t cursor_y = MTY_ZoomGetCursorY(zoom);

	MTY_CursorMove(ctx, cursor_x, cursor_y, scale);
}

MTY_DrawData *MTY_CursorDraw(MTY_Cursor *ctx, MTY_DrawData *dd)
{
	VALIDATE_CTX(ctx, dd);

	if (!ctx->image || !ctx->enabled)
		return dd;

	// We clone the draw data to avoid conflicts with the original one's manipulations
	if (ctx->draw_data) {
		MTY_Free(ctx->draw_data->cmdList);
		MTY_Free(ctx->draw_data);
	}
	ctx->draw_data = MTY_Dup(dd, sizeof(MTY_DrawData));
	ctx->draw_data->cmdList = MTY_Dup(dd->cmdList, dd->cmdListMax * sizeof(MTY_CmdList));

	float x = ctx->x - (ctx->hot_x * ctx->scale);
	float y = ctx->y - (ctx->hot_y * ctx->scale);
	float width = (float) ctx->width * ctx->scale;
	float height = (float) ctx->height * ctx->scale;
	float view_width = ctx->draw_data->displaySize.x;
	float view_height = ctx->draw_data->displaySize.y;

	if (ctx->image_changed) {
		MTY_WindowSetUITexture(ctx->app, ctx->window, CURSOR_ID, ctx->image, ctx->width, ctx->height);
		ctx->image_changed = false;
	}

	ctx->cmd_list->vtx[0].pos = (MTY_Point) {x,         y};
	ctx->cmd_list->vtx[1].pos = (MTY_Point) {x + width, y};
	ctx->cmd_list->vtx[2].pos = (MTY_Point) {x + width, y + height};
	ctx->cmd_list->vtx[3].pos = (MTY_Point) {x,         y + height};

	ctx->cmd_list->cmd[0].clip = (MTY_Rect) {0, 0, view_width, view_height};
	ctx->cmd_list->cmd[1].clip = (MTY_Rect) {0, 0, view_width, view_height};

	ctx->draw_data->cmdListLength++;
	ctx->draw_data->vtxTotalLength += VTX_LEN;
	ctx->draw_data->idxTotalLength += IDX_LEN;

	if (ctx->draw_data->cmdListLength > ctx->draw_data->cmdListMax) {
		ctx->draw_data->cmdListMax = ctx->draw_data->cmdListLength;
		ctx->draw_data->cmdList = MTY_Realloc(ctx->draw_data->cmdList, ctx->draw_data->cmdListMax, sizeof(MTY_CmdList));
	}

	ctx->draw_data->cmdList[ctx->draw_data->cmdListLength - 1] = *ctx->cmd_list;

	return ctx->draw_data;
}
