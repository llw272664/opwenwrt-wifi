// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2023 MediaTek Inc. All Rights Reserved.
 *
 * Author: Alvin Kuo <alvin.kuog@mediatek.com>
 *         Ren-Ting Wang <ren-ting.wang@mediatek.com>
 */

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/relay.h>

#include "trm-fs.h"
#include "trm-mcu.h"
#include "trm.h"

#define RLY_RETRY_NUM				3

static struct dentry *debugfs_dir;
static struct rchan *relay;
static bool trm_fs_is_init;

bool mtk_trm_fs_is_init(void)
{
	return trm_fs_is_init;
}

void *mtk_trm_fs_relay_reserve(u32 size)
{
	u32 rty = 0;
	void *dst;

	while (rty < RLY_RETRY_NUM) {
		dst = relay_reserve(relay, size);
		if (likely(dst))
			return dst;

		rty++;
		msleep(100);
	}

	return ERR_PTR(-ENOMEM);
}

void mtk_trm_fs_relay_flush(void)
{
	relay_flush(relay);
}

static struct dentry *trm_fs_create_buf_file_cb(const char *filename,
						struct dentry *parent,
						umode_t mode,
						struct rchan_buf *buf,
						int *is_global)
{
	struct dentry *debugfs_file;

	debugfs_file = debugfs_create_file("dump_data", mode,
					   parent, buf,
					   &relay_file_operations);

	*is_global = 1;

	return debugfs_file;
}

static int trm_fs_remove_buf_file_cb(struct dentry *debugfs_file)
{
	debugfs_remove(debugfs_file);

	return 0;
}

int mtk_trm_fs_init(void)
{
	static struct rchan_callbacks relay_cb = {
		.create_buf_file = trm_fs_create_buf_file_cb,
		.remove_buf_file = trm_fs_remove_buf_file_cb,
	};
	int ret = 0;

	if (!debugfs_dir) {
		debugfs_dir = debugfs_create_dir("tops", NULL);
		if (IS_ERR(debugfs_dir)) {
			ret = PTR_ERR(debugfs_dir);
			goto out;
		}
	}

	if (!relay) {
		relay = relay_open("dump_data", debugfs_dir,
				   RLY_DUMP_SUBBUF_SZ,
				   RLY_DUMP_SUBBUF_NUM,
				   &relay_cb, NULL);
		if (!relay) {
			ret = -EINVAL;
			goto err_debugfs_remove;
		}
	}

	relay_reset(relay);

	trm_fs_is_init = true;

out:
	return ret;

err_debugfs_remove:
	trm_fs_is_init = false;

	debugfs_remove(debugfs_dir);

	debugfs_dir = NULL;

	return ret;
}

void mtk_trm_fs_deinit(void)
{
	trm_fs_is_init = false;

	relay_close(relay);

	debugfs_remove(debugfs_dir);
}