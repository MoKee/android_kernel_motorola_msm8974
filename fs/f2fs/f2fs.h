/*
 * fs/f2fs/f2fs.h
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *             http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef _LINUX_F2FS_H
#define _LINUX_F2FS_H

#include <linux/types.h>
#include <linux/page-flags.h>
#include <linux/buffer_head.h>
#include <linux/slab.h>
#include <linux/crc32.h>
#include <linux/magic.h>

/*
 * For mount options
 */
#define F2FS_MOUNT_BG_GC		0x00000001
#define F2FS_MOUNT_DISABLE_ROLL_FORWARD	0x00000002
#define F2FS_MOUNT_DISCARD		0x00000004
#define F2FS_MOUNT_NOHEAP		0x00000008
#define F2FS_MOUNT_XATTR_USER		0x00000010
#define F2FS_MOUNT_POSIX_ACL		0x00000020
#define F2FS_MOUNT_DISABLE_EXT_IDENTIFY	0x00000040

#define clear_opt(sbi, option)	(sbi->mount_opt.opt &= ~F2FS_MOUNT_##option)
#define set_opt(sbi, option)	(sbi->mount_opt.opt |= F2FS_MOUNT_##option)
#define test_opt(sbi, option)	(sbi->mount_opt.opt & F2FS_MOUNT_##option)

#define ver_after(a, b)	(typecheck(unsigned long long, a) &&		\
		typecheck(unsigned long long, b) &&			\
		((long long)((a) - (b)) > 0))

typedef u64 block_t;
typedef u32 nid_t;

struct f2fs_mount_info {
	unsigned int	opt;
};

static inline __u32 f2fs_crc32(void *buff, size_t len)
{
	return crc32_le(F2FS_SUPER_MAGIC, buff, len);
}

static inline bool f2fs_crc_valid(__u32 blk_crc, void *buff, size_t buff_size)
{
	return f2fs_crc32(buff, buff_size) == blk_crc;
}

/*
 * For checkpoint manager
 */
enum {
	NAT_BITMAP,
	SIT_BITMAP
};

/* for the list of orphan inodes */
struct orphan_inode_entry {
	struct list_head list;	/* list head */
	nid_t ino;		/* inode number */
};

/* for the list of directory inodes */
struct dir_inode_entry {
	struct list_head list;	/* list head */
	struct inode *inode;	/* vfs inode pointer */
};

/* for the list of fsync inodes, used only during recovery */
struct fsync_inode_entry {
	struct list_head list;	/* list head */
	struct inode *inode;	/* vfs inode pointer */
	block_t blkaddr;	/* block address locating the last inode */
};

#define nats_in_cursum(sum)		(le16_to_cpu(sum->n_nats))
#define sits_in_cursum(sum)		(le16_to_cpu(sum->n_sits))

#define nat_in_journal(sum, i)		(sum->nat_j.entries[i].ne)
#define nid_in_journal(sum, i)		(sum->nat_j.entries[i].nid)
#define sit_in_journal(sum, i)		(sum->sit_j.entries[i].se)
#define segno_in_journal(sum, i)	(sum->sit_j.entries[i].segno)

static inline int update_nats_in_cursum(struct f2fs_summary_block *rs, int i)
{
	int before = nats_in_cursum(rs);
	rs->n_nats = cpu_to_le16(before + i);
	return before;
}

static inline int update_sits_in_cursum(struct f2fs_summary_block *rs, int i)
{
	int before = sits_in_cursum(rs);
	rs->n_sits = cpu_to_le16(before + i);
	return before;
}

/*
 * For INODE and NODE manager
 */
#define XATTR_NODE_OFFSET	(-1)	/*
					 * store xattrs to one node block per
					 * file keeping -1 as its node offset to
					 * distinguish from index node blocks.
					 */
#define RDONLY_NODE		1	/*
					 * specify a read-only mode when getting
					 * a node block. 0 is read-write mode.
					 * used by get_dnode_of_data().
					 */
#define F2FS_LINK_MAX		32000	/* maximum link count per file */

/* for in-memory extent cache entry */
struct extent_info {
	rwlock_t ext_lock;	/* rwlock for consistency */
	unsigned int fofs;	/* start offset in a file */
	u32 blk_addr;		/* start block address of the extent */
	unsigned int len;	/* lenth of the extent */
};

/*
 * i_advise uses FADVISE_XXX_BIT. We can add additional hints later.
 */
#define FADVISE_COLD_BIT	0x01

struct f2fs_inode_info {
	struct inode vfs_inode;		/* serve a vfs inode */
	unsigned long i_flags;		/* keep an inode flags for ioctl */
	unsigned char i_advise;		/* use to give file attribute hints */
	unsigned int i_current_depth;	/* use only in directory structure */
	unsigned int i_pino;		/* parent inode number */
	umode_t i_acl_mode;		/* keep file acl mode temporarily */

	/* Use below internally in f2fs*/
	unsigned long flags;		/* use to pass per-file flags */
	unsigned long long data_version;/* lastes version of data for fsync */
	atomic_t dirty_dents;		/* # of dirty dentry pages */
	f2fs_hash_t chash;		/* hash value of given file name */
	unsigned int clevel;		/* maximum level of given file name */
	nid_t i_xattr_nid;		/* node id that contains xattrs */
	struct extent_info ext;		/* in-memory extent cache entry */
};

static inline void get_extent_info(struct extent_info *ext,
					struct f2fs_extent i_ext)
{
	write_lock(&ext->ext_lock);
	ext->fofs = le32_to_cpu(i_ext.fofs);
	ext->blk_addr = le32_to_cpu(i_ext.blk_addr);
	ext->len = le32_to_cpu(i_ext.len);
	write_unlock(&ext->ext_lock);
}

static inline void set_raw_extent(struct extent_info *ext,
					struct f2fs_extent *i_ext)
{
	read_lock(&ext->ext_lock);
	i_ext->fofs = cpu_to_le32(ext->fofs);
	i_ext->blk_addr = cpu_to_le32(ext->blk_addr);
	i_ext->len = cpu_to_le32(ext->len);
	read_unlock(&ext->ext_lock);
}

struct f2fs_nm_info {
	block_t nat_blkaddr;		/* base disk address of NAT */
	nid_t max_nid;			/* maximum possible node ids */
	nid_t init_scan_nid;		/* the first nid to be scanned */
	nid_t next_scan_nid;		/* the next nid to be scanned */

	/* NAT cache management */
	struct radix_tree_root nat_root;/* root of the nat entry cache */
	rwlock_t nat_tree_lock;		/* protect nat_tree_lock */
	unsigned int nat_cnt;		/* the # of cached nat entries */
	struct list_head nat_entries;	/* cached nat entry list (clean) */
	struct list_head dirty_nat_entries; /* cached nat entry list (dirty) */

	/* free node ids management */
	struct list_head free_nid_list;	/* a list for free nids */
	spinlock_t free_nid_list_lock;	/* protect free nid list */
	unsigned int fcnt;		/* the number of free node id */
	struct mutex build_lock;	/* lock for build free nids */

	/* for checkpoint */
	char *nat_bitmap;		/* NAT bitmap pointer */
	int bitmap_size;		/* bitmap size */
};

/*
 * this structure is used as one of function parameters.
 * all the information are dedicated to a given direct node block determined
 * by the data offset in a file.
 */
struct dnode_of_data {
	struct inode *inode;		/* vfs inode pointer */
	struct page *inode_page;	/* its inode page, NULL is possible */
	struct page *node_page;		/* cached direct node page */
	nid_t nid;			/* node id of the direct node block */
	unsigned int ofs_in_node;	/* data offset in the node page */
	bool inode_page_locked;		/* inode page is locked or not */
	block_t	data_blkaddr;		/* block address of the node block */
};

static inline void set_new_dnode(struct dnode_of_data *dn, struct inode *inode,
		struct page *ipage, struct page *npage, nid_t nid)
{
	memset(dn, 0, sizeof(*dn));
	dn->inode = inode;
	dn->inode_page = ipage;
	dn->node_page = npage;
	dn->nid = nid;
}

/*
 * For SIT manager
 *
 * By default, there are 6 active log areas across the whole main area.
 * When considering hot and cold data separation to reduce cleaning overhead,
 * we split 3 for data logs and 3 for node logs as hot, warm, and cold types,
 * respectively.
 * In the current design, you should not change the numbers intentionally.
 * Instead, as a mount option such as active_logs=x, you can use 2, 4, and 6
 * logs individually according to the underlying devices. (default: 6)
 * Just in case, on-disk layout covers maximum 16 logs that consist of 8 for
 * data and 8 for node logs.
 */
#define	NR_CURSEG_DATA_TYPE	(3)
#define NR_CURSEG_NODE_TYPE	(3)
#define NR_CURSEG_TYPE	(NR_CURSEG_DATA_TYPE + NR_CURSEG_NODE_TYPE)

enum {
	CURSEG_HOT_DATA	= 0,	/* directory entry blocks */
	CURSEG_WARM_DATA,	/* data blocks */
	CURSEG_COLD_DATA,	/* multimedia or GCed data blocks */
	CURSEG_HOT_NODE,	/* direct node blocks of directory files */
	CURSEG_WARM_NODE,	/* direct node blocks of normal files */
	CURSEG_COLD_NODE,	/* indirect node blocks */
	NO_CHECK_TYPE
};

struct f2fs_sm_info {
	struct sit_info *sit_info;		/* whole segment information */
	struct free_segmap_info *free_info;	/* free segment information */
	struct dirty_seglist_info *dirty_info;	/* dirty segment information */
	struct curseg_info *curseg_array;	/* active segment information */

	struct list_head wblist_head;	/* list of under-writeback pages */
	spinlock_t wblist_lock;		/* lock for checkpoint */

	block_t seg0_blkaddr;		/* block address of 0'th segment */
	block_t main_blkaddr;		/* start block address of main area */
	block_t ssa_blkaddr;		/* start block address of SSA area */

	unsigned int segment_count;	/* total # of segments */
	unsigned int main_segments;	/* # of segments in main area */
	unsigned int reserved_segments;	/* # of reserved segments */
	unsigned int ovp_segments;	/* # of overprovision segments */
};

/*
 * For directory operation
 */
#define	NODE_DIR1_BLOCK		(ADDRS_PER_INODE + 1)
#define	NODE_DIR2_BLOCK		(ADDRS_PER_INODE + 2)
#define	NODE_IND1_BLOCK		(ADDRS_PER_INODE + 3)
#define	NODE_IND2_BLOCK		(ADDRS_PER_INODE + 4)
#define	NODE_DIND_BLOCK		(ADDRS_PER_INODE + 5)

/*
 * For superblock
 */
/*
 * COUNT_TYPE for monitoring
 *
 * f2fs monitors the number of several block types such as on-writeback,
 * dirty dentry blocks, dirty node blocks, and dirty meta blocks.
 */
enum count_type {
	F2FS_WRITEBACK,
	F2FS_DIRTY_DENTS,
	F2FS_DIRTY_NODES,
	F2FS_DIRTY_META,
	NR_COUNT_TYPE,
};

/*
 * FS_LOCK nesting subclasses for the lock validator:
 *
 * The locking order between these classes is
 * RENAME -> DENTRY_OPS -> DATA_WRITE -> DATA_NEW
 *    -> DATA_TRUNC -> NODE_WRITE -> NODE_NEW -> NODE_TRUNC
 */
enum lock_type {
	RENAME,		/* for renaming operations */
	DENTRY_OPS,	/* for directory operations */
	DATA_WRITE,	/* for data write */
	DATA_NEW,	/* for data allocation */
	DATA_TRUNC,	/* for data truncate */
	NODE_NEW,	/* for node allocation */
	NODE_TRUNC,	/* for node truncate */
	NODE_WRITE,	/* for node write */
	NR_LOCK_TYPE,
};

/*
 * The below are the page types of bios used in submti_bio().
 * The available types are:
 * DATA			User data pages. It operates as async mode.
 * NODE			Node pages. It operates as async mode.
 * META			FS metadata pages such as SIT, NAT, CP.
 * NR_PAGE_TYPE		The number of page types.
 * META_FLUSH		Make sure the previous pages are written
 *			with waiting the bio's completion
 * ...			Only can be used with META.
 */
enum page_type {
	DATA,
	NODE,
	META,
	NR_PAGE_TYPE,
	META_FLUSH,
};

struct f2fs_sb_info {
	struct super_block *sb;			/* pointer to VFS super block */
	struct buffer_head *raw_super_buf;	/* buffer head of raw sb */
	struct f2fs_super_block *raw_super;	/* raw super block pointer */
	int s_dirty;				/* dirty flag for checkpoint */

	/* for node-related operations */
	struct f2fs_nm_info *nm_info;		/* node manager */
	struct inode *node_inode;		/* cache node blocks */

	/* for segment-related operations */
	struct f2fs_sm_info *sm_info;		/* segment manager */
	struct bio *bio[NR_PAGE_TYPE];		/* bios to merge */
	sector_t last_block_in_bio[NR_PAGE_TYPE];	/* last block number */
	struct rw_semaphore bio_sem;		/* IO semaphore */

	/* for checkpoint */
	struct f2fs_checkpoint *ckpt;		/* raw checkpoint pointer */
	struct inode *meta_inode;		/* cache meta blocks */
	struct mutex cp_mutex;			/* for checkpoint procedure */
	struct mutex fs_lock[NR_LOCK_TYPE];	/* for blocking FS operations */
	struct mutex write_inode;		/* mutex for write inode */
	struct mutex writepages;		/* mutex for writepages() */
	int por_doing;				/* recovery is doing or not */

	/* for orphan inode management */
	struct list_head orphan_inode_list;	/* orphan inode list */
	struct mutex orphan_inode_mutex;	/* for orphan inode list */
	unsigned int n_orphans;			/* # of orphan inodes */

	/* for directory inode management */
	struct list_head dir_inode_list;	/* dir inode list */
	spinlock_t dir_inode_lock;		/* for dir inode list lock */
	unsigned int n_dirty_dirs;		/* # of dir inodes */

	/* basic file system units */
	unsigned int log_sectors_per_block;	/* log2 sectors per block */
	unsigned int log_blocksize;		/* log2 block size */
	unsigned int blocksize;			/* block size */
	unsigned int root_ino_num;		/* root inode number*/
	unsigned int node_ino_num;		/* node inode number*/
	unsigned int meta_ino_num;		/* meta inode number*/
	unsigned int log_blocks_per_seg;	/* log2 blocks per segment */
	unsigned int blocks_per_seg;		/* blocks per segment */
	unsigned int segs_per_sec;		/* segments per section */
	unsigned int secs_per_zone;		/* sections per zone */
	unsigned int total_sections;		/* total section count */
	unsigned int total_node_count;		/* total node block count */
	unsigned int total_valid_node_count;	/* valid node block count */
	unsigned int total_valid_inode_count;	/* valid inode count */
	int active_logs;			/* # of active logs */

	block_t user_block_count;		/* # of user blocks */
	block_t total_valid_block_count;	/* # of valid blocks */
	block_t alloc_valid_block_count;	/* # of allocated blocks */
	block_t last_valid_block_count;		/* for recovery */
	u32 s_next_generation;			/* for NFS support */
	atomic_t nr_pages[NR_COUNT_TYPE];	/* # of pages, see count_type */

	struct f2fs_mount_info mount_opt;	/* mount options */

	/* for cleaning operations */
	struct mutex gc_mutex;			/* mutex for GC */
	struct f2fs_gc_kthread	*gc_thread;	/* GC thread */

	/*
	 * for stat information.
	 * one is for the LFS mode, and the other is for the SSR mode.
	 */
	struct f2fs_stat_info *stat_info;	/* FS status information */
	unsigned int segment_count[2];		/* # of allocated segments */
	unsigned int block_count[2];		/* # of allocated blocks */
	unsigned int last_victim[2];		/* last victim segment # */
	int total_hit_ext, read_hit_ext;	/* extent cache hit ratio */
	int bg_gc;				/* background gc calls */
	spinlock_t stat_lock;			/* lock for stat operations */
};

/*
 * Inline functions
 */
static inline struct f2fs_inode_info *F2FS_I(struct inode *inode)
{
	return container_of(inode, struct f2fs_inode_info, vfs_inode);
}

static inline struct f2fs_sb_info *F2FS_SB(struct super_block *sb)
{
	return sb->s_fs_info;
}

static inline struct f2fs_super_block *F2FS_RAW_SUPER(struct f2fs_sb_info *sbi)
{
	return (struct f2fs_super_block *)(sbi->raw_super);
}

static inline struct f2fs_checkpoint *F2FS_CKPT(struct f2fs_sb_info *sbi)
{
	return (struct f2fs_checkpoint *)(sbi->ckpt);
}

static inline struct f2fs_nm_info *NM_I(struct f2fs_sb_info *sbi)
{
	return (struct f2fs_nm_info *)(sbi->nm_info);
}

static inline struct f2fs_sm_info *SM_I(struct f2fs_sb_info *sbi)
{
	return (struct f2fs_sm_info *)(sbi->sm_info);
}

static inline struct sit_info *SIT_I(struct f2fs_sb_info *sbi)
{
	return (struct sit_info *)(SM_I(sbi)->sit_info);
}

static inline struct free_segmap_info *FREE_I(struct f2fs_sb_info *sbi)
{
	return (struct free_segmap_info *)(SM_I(sbi)->free_info);
}

static inline struct dirty_seglist_info *DIRTY_I(struct f2fs_sb_info *sbi)
{
	return (struct dirty_seglist_info *)(SM_I(sbi)->dirty_info);
}

static inline void F2FS_SET_SB_DIRT(struct f2fs_sb_info *sbi)
{
	sbi->s_dirty = 1;
}

static inline void F2FS_RESET_SB_DIRT(struct f2fs_sb_info *sbi)
{
	sbi->s_dirty = 0;
}

static inline bool is_set_ckpt_flags(struct f2fs_checkpoint *cp, unsigned int f)
{
	unsigned int ckpt_flags = le32_to_cpu(cp->ckpt_flags);
	return ckpt_flags & f;
}

static inline void set_ckpt_flags(struct f2fs_checkpoint *cp, unsigned int f)
{
	unsigned int ckpt_flags = le32_to_cpu(cp->ckpt_flags);
	ckpt_flags |= f;
	cp->ckpt_flags = cpu_to_le32(ckpt_flags);
}

static inline void clear_ckpt_flags(struct f2fs_checkpoint *cp, unsigned int f)
{
	unsigned int ckpt_flags = le32_to_cpu(cp->ckpt_flags);
	ckpt_flags &= (~f);
	cp->ckpt_flags = cpu_to_le32(ckpt_flags);
}

static inline void mutex_lock_op(struct f2fs_sb_info *sbi, enum lock_type t)
{
	mutex_lock_nested(&sbi->fs_lock[t], t);
}

static inline void mutex_unlock_op(struct f2fs_sb_info *sbi, enum lock_type t)
{
	mutex_unlock(&sbi->fs_lock[t]);
}

/*
 * Check whether the given nid is within node id range.
 */
static inline void check_nid_range(struct f2fs_sb_info *sbi, nid_t nid)
{
	BUG_ON((nid >= NM_I(sbi)->max_nid));
}

#define F2FS_DEFAULT_ALLOCATED_BLOCKS	1

/*
 * Check whether the inode has blocks or not
 */
static inline int F2FS_HAS_BLOCKS(struct inode *inode)
{
	if (F2FS_I(inode)->i_xattr_nid)
		return (inode->i_blocks > F2FS_DEFAULT_ALLOCATED_BLOCKS + 1);
	else
		return (inode->i_blocks > F2FS_DEFAULT_ALLOCATED_BLOCKS);
}

static inline bool inc_valid_block_count(struct f2fs_sb_info *sbi,
				 struct inode *inode, blkcnt_t count)
{
	block_t	valid_block_count;

	spin_lock(&sbi->stat_lock);
	valid_block_count =
		sbi->total_valid_block_count + (block_t)count;
	if (valid_block_count > sbi->user_block_count) {
		spin_unlock(&sbi->stat_lock);
		return false;
	}
	inode->i_blocks += count;
	sbi->total_valid_block_count = valid_block_count;
	sbi->alloc_valid_block_count += (block_t)count;
	spin_unlock(&sbi->stat_lock);
	return true;
}

static inline int dec_valid_block_count(struct f2fs_sb_info *sbi,
						struct inode *inode,
						blkcnt_t count)
{
	spin_lock(&sbi->stat_lock);
	BUG_ON(sbi->total_valid_block_count < (block_t) count);
	BUG_ON(inode->i_blocks < count);
	inode->i_blocks -= count;
	sbi->total_valid_block_count -= (block_t)count;
	spin_unlock(&sbi->stat_lock);
	return 0;
}

static inline void inc_page_count(struct f2fs_sb_info *sbi, int count_type)
{
	atomic_inc(&sbi->nr_pages[count_type]);
	F2FS_SET_SB_DIRT(sbi);
}

static inline void inode_inc_dirty_dents(struct inode *inode)
{
	atomic_inc(&F2FS_I(inode)->dirty_dents);
}

static inline void dec_page_count(struct f2fs_sb_info *sbi, int count_type)
{
	atomic_dec(&sbi->nr_pages[count_type]);
}

static inline void inode_dec_dirty_dents(struct inode *inode)
{
	atomic_dec(&F2FS_I(inode)->dirty_dents);
}

static inline int get_pages(struct f2fs_sb_info *sbi, int count_type)
{
	return atomic_read(&sbi->nr_pages[count_type]);
}

static inline block_t valid_user_blocks(struct f2fs_sb_info *sbi)
{
	block_t ret;
	spin_lock(&sbi->stat_lock);
	ret = sbi->total_valid_block_count;
	spin_unlock(&sbi->stat_lock);
	return ret;
}

static inline unsigned long __bitmap_size(struct f2fs_sb_info *sbi, int flag)
{
	struct f2fs_checkpoint *ckpt = F2FS_CKPT(sbi);

	/* return NAT or SIT bitmap */
	if (flag == NAT_BITMAP)
		return le32_to_cpu(ckpt->nat_ver_bitmap_bytesize);
	else if (flag == SIT_BITMAP)
		return le32_to_cpu(ckpt->sit_ver_bitmap_bytesize);

	return 0;
}

static inline void *__bitmap_ptr(struct f2fs_sb_info *sbi, int flag)
{
	struct f2fs_checkpoint *ckpt = F2FS_CKPT(sbi);
	int offset = (flag == NAT_BITMAP) ?
			le32_to_cpu(ckpt->sit_ver_bitmap_bytesize) : 0;
	return &ckpt->sit_nat_version_bitmap + offset;
}

static inline block_t __start_cp_addr(struct f2fs_sb_info *sbi)
{
	block_t start_addr;
	struct f2fs_checkpoint *ckpt = F2FS_CKPT(sbi);
	unsigned long long ckpt_version = le64_to_cpu(ckpt->checkpoint_ver);

	start_addr = le32_to_cpu(F2FS_RAW_SUPER(sbi)->cp_blkaddr);

	/*
	 * odd numbered checkpoint should at cp segment 0
	 * and even segent must be at cp segment 1
	 */
	if (!(ckpt_version & 1))
		start_addr += sbi->blocks_per_seg;

	return start_addr;
}

static inline block_t __start_sum_addr(struct f2fs_sb_info *sbi)
{
	return le32_to_cpu(F2FS_CKPT(sbi)->cp_pack_start_sum);
}

static inline bool inc_valid_node_count(struct f2fs_sb_info *sbi,
						struct inode *inode,
						unsigned int count)
{
	block_t	valid_block_count;
	unsigned int valid_node_count;

	spin_lock(&sbi->stat_lock);

	valid_block_count = sbi->total_valid_block_count + (block_t)count;
	sbi->alloc_valid_block_count += (block_t)count;
	valid_node_count = sbi->total_valid_node_count + count;

	if (valid_block_count > sbi->user_block_count) {
		spin_unlock(&sbi->stat_lock);
		return false;
	}

	if (valid_node_count > sbi->total_node_count) {
		spin_unlock(&sbi->stat_lock);
		return false;
	}

	if (inode)
		inode->i_blocks += count;
	sbi->total_valid_node_count = valid_node_count;
	sbi->total_valid_block_count = valid_block_count;
	spin_unlock(&sbi->stat_lock);

	return true;
}

static inline void dec_valid_node_count(struct f2fs_sb_info *sbi,
						struct inode *inode,
						unsigned int count)
{
	spin_lock(&sbi->stat_lock);

	BUG_ON(sbi->total_valid_block_count < count);
	BUG_ON(sbi->total_valid_node_count < count);
	BUG_ON(inode->i_blocks < count);

	inode->i_blocks -= count;
	sbi->total_valid_node_count -= count;
	sbi->total_valid_block_count -= (block_t)count;

	spin_unlock(&sbi->stat_lock);
}

static inline unsigned int valid_node_count(struct f2fs_sb_info *sbi)
{
	unsigned int ret;
	spin_lock(&sbi->stat_lock);
	ret = sbi->total_valid_node_count;
	spin_unlock(&sbi->stat_lock);
	return ret;
}

static inline void inc_valid_inode_count(struct f2fs_sb_info *sbi)
{
	spin_lock(&sbi->stat_lock);
	BUG_ON(sbi->total_valid_inode_count == sbi->total_node_count);
	sbi->total_valid_inode_count++;
	spin_unlock(&sbi->stat_lock);
}

static inline int dec_valid_inode_count(struct f2fs_sb_info *sbi)
{
	spin_lock(&sbi->stat_lock);
	BUG_ON(!sbi->total_valid_inode_count);
	sbi->total_valid_inode_count--;
	spin_unlock(&sbi->stat_lock);
	return 0;
}

static inline unsigned int valid_inode_count(struct f2fs_sb_info *sbi)
{
	unsigned int ret;
	spin_lock(&sbi->stat_lock);
	ret = sbi->total_valid_inode_count;
	spin_unlock(&sbi->stat_lock);
	return ret;
}

static inline void f2fs_put_page(struct page *page, int unlock)
{
	if (!page || IS_ERR(page))
		return;

	if (unlock) {
		BUG_ON(!PageLocked(page));
		unlock_page(page);
	}
	page_cache_release(page);
}

static inline void f2fs_put_dnode(struct dnode_of_data *dn)
{
	if (dn->node_page)
		f2fs_put_page(dn->node_page, 1);
	if (dn->inode_page && dn->node_page != dn->inode_page)
		f2fs_put_page(dn->inode_page, 0);
	dn->node_page = NULL;
	dn->inode_page = NULL;
}

static inline struct kmem_cache *f2fs_kmem_cache_create(const char *name,
					size_t size, void (*ctor)(void *))
{
	return kmem_cache_create(name, size, 0, SLAB_RECLAIM_ACCOUNT, ctor);
}

#define RAW_IS_INODE(p)	((p)->footer.nid == (p)->footer.ino)

static inline bool IS_INODE(struct page *page)
{
	struct f2fs_node *p = (struct f2fs_node *)page_address(page);
	return RAW_IS_INODE(p);
}

static inline __le32 *blkaddr_in_node(struct f2fs_node *node)
{
	return RAW_IS_INODE(node) ? node->i.i_addr : node->dn.addr;
}

static inline block_t datablock_addr(struct page *node_page,
		unsigned int offset)
{
	struct f2fs_node *raw_node;
	__le32 *addr_array;
	raw_node = (struct f2fs_node *)page_address(node_page);
	addr_array = blkaddr_in_node(raw_node);
	return le32_to_cpu(addr_array[offset]);
}

static inline int f2fs_test_bit(unsigned int nr, char *addr)
{
	int mask;

	addr += (nr >> 3);
	mask = 1 << (7 - (nr & 0x07));
	return mask & *addr;
}

static inline int f2fs_set_bit(unsigned int nr, char *addr)
{
	int mask;
	int ret;

	addr += (nr >> 3);
	mask = 1 << (7 - (nr & 0x07));
	ret = mask & *addr;
	*addr |= mask;
	return ret;
}

static inline int f2fs_clear_bit(unsigned int nr, char *addr)
{
	int mask;
	int ret;

	addr += (nr >> 3);
	mask = 1 << (7 - (nr & 0x07));
	ret = mask & *addr;
	*addr &= ~mask;
	return ret;
}

/* used for f2fs_inode_info->flags */
enum {
	FI_NEW_INODE,		/* indicate newly allocated inode */
	FI_NEED_CP,		/* need to do checkpoint during fsync */
	FI_INC_LINK,		/* need to increment i_nlink */
	FI_ACL_MODE,		/* indicate acl mode */
	FI_NO_ALLOC,		/* should not allocate any blocks */
};

static inline void set_inode_flag(struct f2fs_inode_info *fi, int flag)
{
	set_bit(flag, &fi->flags);
}

static inline int is_inode_flag_set(struct f2fs_inode_info *fi, int flag)
{
	return test_bit(flag, &fi->flags);
}

static inline void clear_inode_flag(struct f2fs_inode_info *fi, int flag)
{
	clear_bit(flag, &fi->flags);
}

static inline void set_acl_inode(struct f2fs_inode_info *fi, umode_t mode)
{
	fi->i_acl_mode = mode;
	set_inode_flag(fi, FI_ACL_MODE);
}

static inline int cond_clear_inode_flag(struct f2fs_inode_info *fi, int flag)
{
	if (is_inode_flag_set(fi, FI_ACL_MODE)) {
		clear_inode_flag(fi, FI_ACL_MODE);
		return 1;
	}
	return 0;
}

/*
 * file.c
 */
int f2fs_sync_file(struct file *, loff_t, loff_t, int);
void truncate_data_blocks(struct dnode_of_data *);
void f2fs_truncate(struct inode *);
int f2fs_setattr(struct dentry *, struct iattr *);
int truncate_hole(struct inode *, pgoff_t, pgoff_t);
long f2fs_ioctl(struct file *, unsigned int, unsigned long);

/*
 * inode.c
 */
void f2fs_set_inode_flags(struct inode *);
struct inode *f2fs_iget_nowait(struct super_block *, unsigned long);
struct inode *f2fs_iget(struct super_block *, unsigned long);
void update_inode(struct inode *, struct page *);
int f2fs_write_inode(struct inode *, struct writeback_control *);
void f2fs_evict_inode(struct inode *);

/*
 * namei.c
 */
struct dentry *f2fs_get_parent(struct dentry *child);

/*
 * dir.c
 */
struct f2fs_dir_entry *f2fs_find_entry(struct inode *, struct qstr *,
							struct page **);
struct f2fs_dir_entry *f2fs_parent_dir(struct inode *, struct page **);
ino_t f2fs_inode_by_name(struct inode *, struct qstr *);
void f2fs_set_link(struct inode *, struct f2fs_dir_entry *,
				struct page *, struct inode *);
void init_dent_inode(struct dentry *, struct page *);
int f2fs_add_link(struct dentry *, struct inode *);
void f2fs_delete_entry(struct f2fs_dir_entry *, struct page *, struct inode *);
int f2fs_make_empty(struct inode *, struct inode *);
bool f2fs_empty_dir(struct inode *);

/*
 * super.c
 */
int f2fs_sync_fs(struct super_block *, int);
extern __printf(3, 4)
void f2fs_msg(struct super_block *, const char *, const char *, ...);

/*
 * hash.c
 */
f2fs_hash_t f2fs_dentry_hash(const char *, size_t);

/*
 * node.c
 */
struct dnode_of_data;
struct node_info;

int is_checkpointed_node(struct f2fs_sb_info *, nid_t);
void get_node_info(struct f2fs_sb_info *, nid_t, struct node_info *);
int get_dnode_of_data(struct dnode_of_data *, pgoff_t, int);
int truncate_inode_blocks(struct inode *, pgoff_t);
int remove_inode_page(struct inode *);
int new_inode_page(struct inode *, struct dentry *);
struct page *new_node_page(struct dnode_of_data *, unsigned int);
void ra_node_page(struct f2fs_sb_info *, nid_t);
struct page *get_node_page(struct f2fs_sb_info *, pgoff_t);
struct page *get_node_page_ra(struct page *, int);
void sync_inode_page(struct dnode_of_data *);
int sync_node_pages(struct f2fs_sb_info *, nid_t, struct writeback_control *);
bool alloc_nid(struct f2fs_sb_info *, nid_t *);
void alloc_nid_done(struct f2fs_sb_info *, nid_t);
void alloc_nid_failed(struct f2fs_sb_info *, nid_t);
void recover_node_page(struct f2fs_sb_info *, struct page *,
		struct f2fs_summary *, struct node_info *, block_t);
int recover_inode_page(struct f2fs_sb_info *, struct page *);
int restore_node_summary(struct f2fs_sb_info *, unsigned int,
				struct f2fs_summary_block *);
void flush_nat_entries(struct f2fs_sb_info *);
int build_node_manager(struct f2fs_sb_info *);
void destroy_node_manager(struct f2fs_sb_info *);
int create_node_manager_caches(void);
void destroy_node_manager_caches(void);

/*
 * segment.c
 */
void f2fs_balance_fs(struct f2fs_sb_info *);
void invalidate_blocks(struct f2fs_sb_info *, block_t);
void locate_dirty_segment(struct f2fs_sb_info *, unsigned int);
void clear_prefree_segments(struct f2fs_sb_info *);
int npages_for_summary_flush(struct f2fs_sb_info *);
void allocate_new_segments(struct f2fs_sb_info *);
struct page *get_sum_page(struct f2fs_sb_info *, unsigned int);
struct bio *f2fs_bio_alloc(struct block_device *, int);
void f2fs_submit_bio(struct f2fs_sb_info *, enum page_type, bool sync);
int write_meta_page(struct f2fs_sb_info *, struct page *,
					struct writeback_control *);
void write_node_page(struct f2fs_sb_info *, struct page *, unsigned int,
					block_t, block_t *);
void write_data_page(struct inode *, struct page *, struct dnode_of_data*,
					block_t, block_t *);
void rewrite_data_page(struct f2fs_sb_info *, struct page *, block_t);
void recover_data_page(struct f2fs_sb_info *, struct page *,
				struct f2fs_summary *, block_t, block_t);
void rewrite_node_page(struct f2fs_sb_info *, struct page *,
				struct f2fs_summary *, block_t, block_t);
void write_data_summaries(struct f2fs_sb_info *, block_t);
void write_node_summaries(struct f2fs_sb_info *, block_t);
int lookup_journal_in_cursum(struct f2fs_summary_block *,
					int, unsigned int, int);
void flush_sit_entries(struct f2fs_sb_info *);
int build_segment_manager(struct f2fs_sb_info *);
void reset_victim_segmap(struct f2fs_sb_info *);
void destroy_segment_manager(struct f2fs_sb_info *);

/*
 * checkpoint.c
 */
struct page *grab_meta_page(struct f2fs_sb_info *, pgoff_t);
struct page *get_meta_page(struct f2fs_sb_info *, pgoff_t);
long sync_meta_pages(struct f2fs_sb_info *, enum page_type, long);
int check_orphan_space(struct f2fs_sb_info *);
void add_orphan_inode(struct f2fs_sb_info *, nid_t);
void remove_orphan_inode(struct f2fs_sb_info *, nid_t);
int recover_orphan_inodes(struct f2fs_sb_info *);
int get_valid_checkpoint(struct f2fs_sb_info *);
void set_dirty_dir_page(struct inode *, struct page *);
void remove_dirty_dir_inode(struct inode *);
void sync_dirty_dir_inodes(struct f2fs_sb_info *);
void block_operations(struct f2fs_sb_info *);
void write_checkpoint(struct f2fs_sb_info *, bool, bool);
void init_orphan_info(struct f2fs_sb_info *);
int create_checkpoint_caches(void);
void destroy_checkpoint_caches(void);

/*
 * data.c
 */
int reserve_new_block(struct dnode_of_data *);
void update_extent_cache(block_t, struct dnode_of_data *);
struct page *find_data_page(struct inode *, pgoff_t);
struct page *get_lock_data_page(struct inode *, pgoff_t);
struct page *get_new_data_page(struct inode *, pgoff_t, bool);
int f2fs_readpage(struct f2fs_sb_info *, struct page *, block_t, int);
int do_write_data_page(struct page *);

/*
 * gc.c
 */
int start_gc_thread(struct f2fs_sb_info *);
void stop_gc_thread(struct f2fs_sb_info *);
block_t start_bidx_of_node(unsigned int);
int f2fs_gc(struct f2fs_sb_info *);
void build_gc_manager(struct f2fs_sb_info *);
int create_gc_caches(void);
void destroy_gc_caches(void);

/*
 * recovery.c
 */
void recover_fsync_data(struct f2fs_sb_info *);
bool space_for_roll_forward(struct f2fs_sb_info *);

/*
 * debug.c
 */
#ifdef CONFIG_F2FS_STAT_FS
struct f2fs_stat_info {
	struct list_head stat_list;
	struct f2fs_sb_info *sbi;
	struct mutex stat_lock;
	int all_area_segs, sit_area_segs, nat_area_segs, ssa_area_segs;
	int main_area_segs, main_area_sections, main_area_zones;
	int hit_ext, total_ext;
	int ndirty_node, ndirty_dent, ndirty_dirs, ndirty_meta;
	int nats, sits, fnids;
	int total_count, utilization;
	int bg_gc;
	unsigned int valid_count, valid_node_count, valid_inode_count;
	unsigned int bimodal, avg_vblocks;
	int util_free, util_valid, util_invalid;
	int rsvd_segs, overp_segs;
	int dirty_count, node_pages, meta_pages;
	int prefree_count, call_count;
	int tot_segs, node_segs, data_segs, free_segs, free_secs;
	int tot_blks, data_blks, node_blks;
	int curseg[NR_CURSEG_TYPE];
	int cursec[NR_CURSEG_TYPE];
	int curzone[NR_CURSEG_TYPE];

	unsigned int segment_count[2];
	unsigned int block_count[2];
	unsigned base_mem, cache_mem;
};

#define stat_inc_call_count(si)	((si)->call_count++)

#define stat_inc_seg_count(sbi, type)					\
	do {								\
		struct f2fs_stat_info *si = sbi->stat_info;		\
		(si)->tot_segs++;					\
		if (type == SUM_TYPE_DATA)				\
			si->data_segs++;				\
		else							\
			si->node_segs++;				\
	} while (0)

#define stat_inc_tot_blk_count(si, blks)				\
	(si->tot_blks += (blks))

#define stat_inc_data_blk_count(sbi, blks)				\
	do {								\
		struct f2fs_stat_info *si = sbi->stat_info;		\
		stat_inc_tot_blk_count(si, blks);			\
		si->data_blks += (blks);				\
	} while (0)

#define stat_inc_node_blk_count(sbi, blks)				\
	do {								\
		struct f2fs_stat_info *si = sbi->stat_info;		\
		stat_inc_tot_blk_count(si, blks);			\
		si->node_blks += (blks);				\
	} while (0)

int f2fs_build_stats(struct f2fs_sb_info *);
void f2fs_destroy_stats(struct f2fs_sb_info *);
void destroy_root_stats(void);
#else
#define stat_inc_call_count(si)
#define stat_inc_seg_count(si, type)
#define stat_inc_tot_blk_count(si, blks)
#define stat_inc_data_blk_count(si, blks)
#define stat_inc_node_blk_count(sbi, blks)

static inline int f2fs_build_stats(struct f2fs_sb_info *sbi) { return 0; }
static inline void f2fs_destroy_stats(struct f2fs_sb_info *sbi) { }
static inline void destroy_root_stats(void) { }
#endif

extern const struct file_operations f2fs_dir_operations;
extern const struct file_operations f2fs_file_operations;
extern const struct inode_operations f2fs_file_inode_operations;
extern const struct address_space_operations f2fs_dblock_aops;
extern const struct address_space_operations f2fs_node_aops;
extern const struct address_space_operations f2fs_meta_aops;
extern const struct inode_operations f2fs_dir_inode_operations;
extern const struct inode_operations f2fs_symlink_inode_operations;
extern const struct inode_operations f2fs_special_inode_operations;
#endif