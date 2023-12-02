#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <fcntl.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>

#include "fs.h"

#define MAGIC 0xdcc605f5 // esse valor está no fs.h
#define ERROR -1

struct superblock * create_superblock(const char *fname, uint64_t blocksize, uint64_t numero_blocos){
    struct superblock *sb;
    sb = (struct superblock *)malloc(blocksize);
    sb->magic = MAGIC;
    sb->blks = numero_blocos;
    sb->blksz = blocksize;
    sb->freeblks = numero_blocos - 3;
    sb->root = 2; // localização do diretório raiz
    sb->freelist = 3; // localização da lista de blocos
    sb->fd = open(fname, O_RDWR, 0777);
    return sb; 
}

struct inode * create_root_directory(uint64_t blocksize){
    struct inode *root;
    root = (struct inode *)malloc(blocksize); 
    root->mode = IMDIR;
    root->parent = 2;
    root->meta = 1;
    root->next = 0;
    memset(root->links, 0, sizeof(uint64_t));
    return root; 
}

/* Build a new filesystem image in =fname (the file =fname should be present
 * in the OS's filesystem).  The new filesystem should use =blocksize as its
 * block size; the number of blocks in the filesystem will be automatically
 * computed from the file size.  The filesystem will be initialized with an
 * empty root directory.  This function returns NULL on error and sets errno
 * to the appropriate error code.  If the block size is smaller than
 * MIN_BLOCK_SIZE bytes, then the format fails and the function sets errno to
 * EINVAL.  If there is insufficient space to store MIN_BLOCK_COUNT blocks in
 * =fname, then the function fails and sets errno to ENOSPC. */
struct superblock *fs_format(const char *fname, uint64_t blocksize)
{

    // verifica se o tamanho do bloco é menor do que o tamanho mínimo de bloco
    if (blocksize < MIN_BLOCK_SIZE)
    {
        errno = EINVAL;
        return NULL;
    }

    FILE *arquivo = fopen(fname, "r");

    // fazendo o ponteiro apontar para o final do arquivo
    fseek(arquivo, 0, SEEK_END);
    long long tamanho = ftell(arquivo);
    fclose(arquivo);

    uint64_t numero_blocos = tamanho / blocksize;

    // verifica se o numero de blocos é menor do que o numero mínimo de blocos
    if (numero_blocos < MIN_BLOCK_COUNT)
    {
        errno = ENOSPC;
        return NULL;
    }

    struct superblock *sb = create_superblock(fname, blocksize, numero_blocos);
    struct inode *root = create_root_directory(blocksize); 
    struct nodeinfo *rootinfo = (struct nodeinfo *) malloc(blocksize);

    rootinfo->size = 0; 
    strcpy(rootinfo->name, "/\0");

    write(sb->fd, sb, blocksize); 
    write(sb->fd, rootinfo, blocksize); 
    write(sb->fd, root, blocksize); 

    struct freepage *fp; 
    // inicializando a lista de páginas vazias
    for(int i = 4; i < blocksize-1; i++){
        fp->next = i; 
        write(sb->fd, fp, blocksize); 
    }

    fp->next = 0; 
    write(sb->fd, fp, blocksize); 

    return sb;
}

/* Open the filesystem in =fname and return its superblock.  Returns NULL on
 * error, and sets errno accordingly.  If =fname does not contain a
 * 0xdcc605fs, then errno is set to EBADF. */
struct superblock *fs_open(const char *fname) {}

/* Close the filesystem pointed to by =sb.  Returns zero on success and a
 * negative number on error.  If there is an error, all resources are freed
 * and errno is set appropriately. */
int fs_close(struct superblock *sb)
{
    if (sb->magic != MAGIC)
    {
        errno = EBADF;
        return ERROR;
    }

    close(sb->fd);
    free(sb);

    return 0;
}

/* Get a free block in the filesystem.  This block shall be removed from the
 * list of free blocks in the filesystem.  If there are no free blocks, zero
 * is returned.  If an error occurs, (uint64_t)-1 is returned and errno is set
 * appropriately. */
uint64_t fs_get_block(struct superblock *sb) {}

/* Put =block back into the filesystem as a free block.  Returns zero on
 * success or a negative value on error.  If there is an error, errno is set
 * accordingly. */
int fs_put_block(struct superblock *sb, uint64_t block) {}

int fs_write_file(struct superblock *sb, const char *fname, char *buf,
                  size_t cnt) {}

ssize_t fs_read_file(struct superblock *sb, const char *fname, char *buf,
                     size_t bufsz) {}

int fs_unlink(struct superblock *sb, const char *fname) {}

int fs_mkdir(struct superblock *sb, const char *dname) {}

int fs_rmdir(struct superblock *sb, const char *dname) {}

char *fs_list_dir(struct superblock *sb, const char *dname) {}
