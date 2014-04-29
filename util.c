#include "util.h"

// util.h
//extern INODE myinode;
extern PROC proc[2];
extern PROC* running;
extern PROC* readyQueue;
extern MINODE minode[NMINODES];
extern MINODE* root;

extern char pathName[256];
extern char parameter[256];
extern char baseName[128];
extern char dirName[128];

extern char pathNameTokenized[256];
extern char *pathNameTokenPtrs[256];
extern int tokenCount;


int get_block(int fd, int blk, char *buf)
{
    lseek(fd, (long)(blk * BLOCK_SIZE), 0);
    return read(fd, buf, BLOCK_SIZE);  // return: -1 (error)
                                       //          0 (EOF)
}


int put_block(int dev, int blk, char* buf)
{
    lseek(dev, (long)(blk * BLOCK_SIZE), 0);
    return write(dev, buf, BLOCK_SIZE);  // return: -1 (error)
}


int token_path(char *pathname, char **token_ptrs)
{
    int tok_i;
    token_ptrs[0] = strtok(pathname, "/\n");

    for (tok_i = 0; token_ptrs[tok_i]; tok_i++)
    {
        token_ptrs[tok_i + 1] = strtok(NULL, "/\n");
    }
    return tok_i;
}


char* dir_name(char* pathname)
{
    char temp[256];
    bzero(temp, 256);
    strncpy(temp, pathname, strlen(pathname) + 1);
    strncpy(dirName, dirname(temp), strlen(temp) + 1);
    return dirName;
}


char* base_name(char* pathname)
{
    char temp[256];
    bzero(temp, 256);
    strncpy(temp, pathname, strlen(pathname) + 1);
    strncpy(baseName, basename(temp), strlen(temp) + 1);
    return baseName;
}


u32 getino (int* dev, char* pathname)
{

    MINODE* mip;
    u32 ino;
    int i, workingDev;
    char* token;

    if ('/' == pathname[0])
    {
        ino = root->ino;
        *dev = root->dev;
    }
    else
    {
        ino = running->cwd->ino;
        *dev = running->cwd->dev;
    }

    strncpy(pathNameTokenized, pathname, strlen(pathname) + 1);
    tokenCount = token_path(pathNameTokenized, pathNameTokenPtrs);

    if (tokenCount == 0) return -1;

    for (i = 0; i < tokenCount; i++)
    {
        mip = iget(*dev, ino);
        ino = search(mip, pathNameTokenPtrs[i]);
        *dev = mip->dev;
        if (-1 == ino)
            return -1;
    }

    return ino;
}


u32 search (MINODE* mip, char* name)
{
    int i;
    char *cp;
    char buf[BLOCK_SIZE];
    char temp[128];

    ip = &(mip->INODE);

    for (i = 0; i < EXT2_NDIR_BLOCKS; i++)
    {
        if (0 == ip->i_block[i]) break;

        get_block(mip->dev, ip->i_block[i], buf);
        dp = (DIR*)buf;
        cp = buf;

        //printf("i=%d i_block[%d]=%d\n\n", i, i, ip->i_block[i]);
        //printf("   i_number rec_len name_len   name\n");

        while (cp < (buf + BLOCK_SIZE))
        {
            strncpy(temp, dp->name, dp->name_len + 1);
            temp[dp->name_len] = 0;
            //printf("   %5d    %4d    %4d       %s\n", dp->inode, dp->rec_len, dp->name_len, temp);

            if (0 == strcmp(name, temp))
            {
                if (EXT2_FT_DIR != dp->file_type) return -1; // Ensure it is a DIR
                //printf("found %s : ino = %d\n", temp, dp->inode);
                return dp->inode;
            }
            cp += dp->rec_len;
            dp = (DIR*)cp;
        }
    }
    return -1;
}


MINODE* iget (int dev, unsigned long ino)
{
    int i;
    int blk, offset;
    char buf[BLOCK_SIZE];

    // Ensure INODE is not already loaded
    // search minode[] for an entry with (dev,ino) AND refCount > 0;
    for (i = 0; i < NMINODES; i++)
    {
        if (minode[i].dev == dev && minode[i].ino == ino && minode[i].refCount > 0)
        {
            minode[i].refCount++;
            return &minode[i];
        }
    }

    // Get INODE of (dev, ino)
    blk = (ino - 1)/8 + INODEBLOCK;
    offset = (ino - 1) % 8;
    get_block(dev, blk, buf);
    ip = (INODE*)buf + offset;

    // not found, use a FREE minode[i] to load the INODE of (dev, ino)
    for (i = 0; i < NMINODES; i++)
    {
        // FREE minode
        if (minode[i].refCount == 0)
        {
            minode[i].INODE = *ip; // load INODE of (dev,ino)
            minode[i].dev = dev;
            minode[i].ino = ino;
            minode[i].refCount = 1;
            minode[i].dirty = 0;
            minode[i].mounted = 0;  // NEEDS TO BE DONE
            minode[i].mountptr = 0; // NEEDS TO BE DONE
            return &minode[i];
        }
    }

    return NULL; // minode is full!
}

void iput (MINODE* mip)
{
    int blk, offset;
    char buf[BLOCK_SIZE];

    mip->refCount--;
    if (0 < mip->refCount) { return; }
    if (!mip->dirty) { return; }
    if (0 == mip->refCount && mip->dirty)
    {
        // write INODE back to disk by its (dev, ino)

        // Get INODE of (dev, ino)
        blk = (mip->ino - 1)/8 + INODEBLOCK;
        offset = (mip->ino - 1) % 8;

        lseek(mip->dev, (long)(blk * BLOCK_SIZE) + offset, 0);
        write(mip->dev, mip->INODE, sizeof(INODE));
    }
}

int findmyname (MINODE* parent, u32 myino, char* myname)
{
    int i;
    char* cp;
    char buf[BLOCK_SIZE];

    ip = &(parent->INODE);

    for (i = 0; i < EXT2_NDIR_BLOCKS; i++)
    {
        if (0 == ip->i_block[i]) break;

        get_block(parent->dev, ip->i_block[i], buf);
        dp = (DIR*)buf;
        cp = buf;

        printf("i=%d i_block[%d]=%d\n\n", i, i, ip->i_block[i]);
        printf("   i_number rec_len name_len   name\n");

        while (cp < (buf + BLOCK_SIZE))
        {
            strncpy(myname, dp->name, dp->name_len + 1);
            myname[dp->name_len] = 0;
            printf("   %5d    %4d    %4d       %s\n", dp->inode, dp->rec_len, dp->name_len, myname);

            if (dp->inode == myino)
            {
                printf("found ino = %d\n", dp->inode);
                return dp->inode;
            }
            cp += dp->rec_len;
            dp = (DIR*)cp;
        }
    }
    return -1;
}

int findino (MINODE* mip, u32* myino, u32* parent)
{
    char *cp;
    char buf[BLOCK_SIZE];

    ip = &(mip->INODE);

    if (0 == ip->i_block[0]) return -1;
    get_block (mip->dev, ip->i_block[0], buf);
    dp = (DIR*)buf;
    cp = buf;

    *myino = dp->inode;
    cp += dp->rec_len;
    dp = (DIR*)cp;
    *parent = dp->inode;

    return 0;
}

void incFreeInodes(int dev)
{
    char buf[BLOCK_SIZE];

    // Fix inode count in super block.
    get_block(dev, SUPERBLOCK, buf);
    sp = (SUPER *) buf;
    sp->s_free_inodes_count++;
    put_block(dev, SUPERBLOCK, buf);

    // Fix inode count in group descriptor block.
    get_block(dev, GDBLOCK, buf);
    gp = (GD *) buf;
    gp->bg_free_inodes_count++;
    put_block(dev, GDBLOCK, buf);
}

void idealloc(int dev, u32 ino)
{
    int i;
    char buf[BLOCK_SIZE];

    // get inode bitmap block
    get_block(dev, IBITMAP, buf);      // assume Imap is block 4
    CLR_bit(buf, ino-1);         // assume you have clr_bit() function 

    // write buf back
    put_block(dev, IBITMAP, buf);

    // update free inode count in SUPER and GD
    incFreeInodes(dev);         // assume you write this function
}

void incFreeBlocks(int dev)
{
    char buf[BLOCK_SIZE];

    // Fix block count in super block.
    get_block(dev, SUPERBLOCK, buf);
    sp = (SUPER *) buf;
    sp->s_free_blocks_count++;
    put_block(dev, SUPERBLOCK, buf);

    // Fix block count in group descriptor block.
    get_block(dev, GDBLOCK, buf);
    gp = (GD *) buf;
    gp->bg_free_blocks_count++;
    put_block(dev, GDBLOCK, buf);
}

void bdealloc(int dev, u32 blk)
{
    int i;
    char buf[BLOCK_SIZE];

    // get block bitmap block
    get_block(dev, BBITMAP, buf);      // assume Bmap is block 3
    CLR_bit(buf, blk-1);         // assume you have clr_bit() function 

    // write buf back
    put_block(dev, BBITMAP, buf);

    // update free block count in SUPER and GD
    incFreeBlocks(dev);         // assume you write this function
}

void decFreeInodes(int dev)
{
    char buf[BLOCK_SIZE];

    // Fix inode count in super block.
    get_block(dev, SUPERBLOCK, buf);
    sp = (SUPER *) buf;
    sp->s_free_inodes_count--;
    put_block(dev, SUPERBLOCK, buf);

    // Fix inode count in group descriptor block.
    get_block(dev, GDBLOCK, buf);
    gp = (GD *) buf;
    gp->bg_free_inodes_count--;
    put_block(dev, GDBLOCK, buf);
}

int ialloc (int dev)
{
    int i;
    char buf[BLOCK_SIZE];
    u32 ninodes; // FIXME needs to be replaced from MOUNT struct ninodes

    get_block(dev, SUPERBLOCK, buf);
    sp = (SUPER*)buf;
    ninodes = sp->s_inodes_count;

    get_block(dev, IBITMAP, buf);
    for (i = 0; i < ninodes; i++)
    {
        if (TST_bit(buf, i) == 0)
        {
            SET_bit(buf, i);
            put_block(dev, IBITMAP, buf);

            decFreeInodes(dev);
            return (i + 1);
        }
    }
    return 0;
}

void decFreeBlocks(int dev)
{
    char buf[BLOCK_SIZE];

    // Fix block count in super block.
    get_block(dev, SUPERBLOCK, buf);
    sp = (SUPER *) buf;
    sp->s_free_blocks_count--;
    put_block(dev, SUPERBLOCK, buf);

    // Fix block count in group descriptor block.
    get_block(dev, GDBLOCK, buf);
    gp = (GD *) buf;
    gp->bg_free_blocks_count--;
    put_block(dev, GDBLOCK, buf);
}

int balloc (int dev)
{
    int i;
    char buf[BLOCK_SIZE];
    u32 nblocks; //FIXME needs to replaced from MOUNT struct bnodes

    get_block(dev, SUPERBLOCK, buf);
    sp = (SUPER*)buf;
    nblocks = sp->s_blocks_count;

    get_block(dev, BBITMAP, buf);
    for (i = 0; i < nblocks; i++)
    {
        if (TST_bit(buf, i) == 0)
        {
            SET_bit(buf, i);
            put_block(dev, IBITMAP, buf);

            decFreeBlocks(dev);
            return (i + 1);
        }
    }
    return 0;
}

int TST_bit (char buf[], int BIT)
{
    return buf[BIT/8] & (1 << (BIT%8));
}

int SET_bit (char buf[], int BIT)
{
    return buf[BIT/8] |= (1 << (BIT%8));
}

int CLR_bit (char buf[], int BIT)
{
    return buf[BIT/8] &= ~(1 << (BIT%8));
}
