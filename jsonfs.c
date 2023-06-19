/* taken from https://github.com/fntlnz/fuse-example, and modified */


#define FUSE_USE_VERSION 26

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fuse.h>
#include <json.h>
#include <errno.h>
#include <unistd.h>

#define MAX_DATA_LENGTH 4098

typedef enum _FileType {
  DIRECTORY,
  REGULAR_FILE 
} FileType ;

typedef struct _FileSystemNode {
  int inode ;
  FileType type ;
  char name[MAX_DATA_LENGTH] ;
  char data[MAX_DATA_LENGTH] ;
  FileSystemNode * firstChild ;
  FileSystemNode * nextSibling ;
} FileSystemNode ;

FileSystemNode * createNode (int inode, FileType type, char * name, char * data) 
{
  FileSystemNode * newNode = (FileSystemNode *)malloc(sizeof(FileSystemNode)) ;
  if (newNode != NULL) {
    newNode->inode = inode ;
    newNode->type = type ;
    memcpy(newNode->name, name, strlen(name) + 1) ;
    if (type == REGULAR_FILE) {
      memcpy(newNode->data, data, strlen(data) + 1) ;
    }
    else {
      newNode->data = "" ;
    }
    newNode->firstChild = NULL ;
    newNode->nextSibling = NULL ;
  }

  return newNode ;
}

void addChild (FileSystemNode * parent, FileSystemNode * child) 
{
  if (parent == NULL || child == NULL) {
    return ;
  }

  if (parent->firstChild == NULL) {
    parent->firstChild = child ;
  }
  else {
    FileSystemNode * sibling = parent->firstChild ;
    while (sibling->nextSibling != NULL) {
      sibling = sibling->nextSibling ;
    }
    sibling->nextSibling = child ;
  }
}

FileType getType (struct json_object * json)
{
  
}


static const char *filecontent = "I'm the content of the only file available there\n";

// reading metadata of a given path
static int getattr_callback(const char *path, struct stat *stbuf) {
  memset(stbuf, 0, sizeof(struct stat));

  if (strcmp(path, "/") == 0) { // if path == root, declare it as a directory and return
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 2;
    return 0;
  }

  // TODO : /file 수정해야함
  if (strcmp(path, "/file") == 0) { // if path == /file, declare it as a file and explicit its size and return
    stbuf->st_mode = S_IFREG | 0777;
    stbuf->st_nlink = 1;
    stbuf->st_size = strlen(filecontent);
    return 0;
  }

  return -ENOENT;
}

// telling FUSE the exact structure of the accessed directory
static int readdir_callback(const char *path, void *buf, fuse_fill_dir_t filler,
    off_t offset, struct fuse_file_info *fi) {

  if (strcmp(path, "/") == 0) {

	  filler(buf, ".", NULL, 0);
	  filler(buf, "..", NULL, 0);
	  filler(buf, "file", NULL, 0);
    // TODO : "file" 대신 DS 로 filler 채워야 함
	  return 0;
  }

  return -ENOENT ;
}

static int open_callback(const char *path, struct fuse_file_info *fi) {
  return 0;
}

// reading data from an opened file
static int read_callback(const char *path, char *buf, size_t size, off_t offset,
    struct fuse_file_info *fi) {

  if (strcmp(path, "/file") == 0) {
    size_t len = strlen(filecontent);
    if (offset >= len) {
      return 0;
    }

    if (offset + size > len) {
      memcpy(buf, filecontent + offset, len - offset);
      return len - offset;
    }

    memcpy(buf, filecontent + offset, size);
    return size;
  }

  return -ENOENT;
}

static struct fuse_operations fuse_example_operations = {
  .getattr = getattr_callback,
  .open = open_callback,
  .read = read_callback,
  .readdir = readdir_callback,
};

void json_to_ds (struct json_object * json, FileSystemNode * fs) 
{
  int n = json_object_array_length(json);

	for ( int i = 0 ; i < n ; i++ ) {
		struct json_object * obj = json_object_array_get_idx(json, i);
    int inode ;
    FileType type ;
    char name[MAX_DATA_LENGTH] = "", data[MAX_DATA_LENGTH] = "" ;
    
    json_object_object_foreach(obj, key, val) {  
      if (strcmp(key, "inode") == 0) 
				inode = (int) json_object_get_int(val) ;

			if (strcmp(key, "type") == 0) {
        const char * typeString = (char *) json_object_get_string(val) ;
        if (strcmp(typeString, "dir") == 0) {
          type = DIRECTORY ;
        }
        else {
          type = REGULAR_FILE ;
        }
      }

			if (strcmp(key, "name" ) == 0)
				memcpy(name, (char *) json_object_get_string(val), strlen((char *) json_object_get_string(val)) + 1) ;
			
			if (strcmp(key, "data" ) == 0)
				memcpy(data, (char *) json_object_get_string(val), strlen((char *) json_object_get_string(val)) + 1) ;  
		}
    FileSystemNode * temp = createNode(inode, type, name, data) ;
    addChild(fs, temp) ;
  }

}

void print_fs (FileSystemNode * fs, int level)
{
  if (fs == NULl)
    return ;

  for (int i = 0; i < level; i++) {
    printf("  ") ;
  }
  printf("Name: %s\n", fs->name) ;
  printf("Inode: %d\n", fs->inode) ;
  printf("Type: %s\n", fs->type == DIRECTORY ? "Directory" : "Regular File") ;
  if (node->type == REGULAR_FILE) {
        printf("Data: %s\n", fs->data) ;
  }

	// Print the children recursively
  FileSystemNode * child = fs->firstChild ;
  while (child != NULL) {
      print_fs(child, level + 1) ;
      child = child->nextSibling ;
  }
}

void print_json (struct json_object * json) 
{
	int n = json_object_array_length(json);

	for ( int i = 0 ; i < n ; i++ ) {
		struct json_object * obj = json_object_array_get_idx(json, i);

		printf("{\n") ;
		json_object_object_foreach(obj, key, val) {
			if (strcmp(key, "inode") == 0) 
				printf("   inode: %d\n", (int) json_object_get_int(val)) ;

			if (strcmp(key, "type") == 0) 
				printf("   type: %s\n", (char *) json_object_get_string(val)) ;

			if (strcmp(key, "name" ) == 0)
				printf("   name: %s\n", (char *) json_object_get_string(val)) ;
			
			if (strcmp(key, "entries") == 0) 
				printf("   # entries: %d\n", json_object_array_length(val)) ;
		}
		printf("}\n") ;
	}
}


int main(int argc, char *argv[])
{
  char *filename = NULL ;
  if (argc < 2) {
    printf("Name of a JSON file is required!\n") ;
    return 1 ;
  } else {
    filename = (char *)malloc(strlen(argv[1]) + 1) ;
    if (filename == NULL) {
      printf("Memory allocation failed.\n") ;
      return 1 ;
    }
    memcpy(filename, argv[1], strlen(argv[1]) + 1) ;
  }

	struct json_object * fs_json = json_object_from_file(filename) ;
  if (fs_json == NULL) {
    printf("Failed to load JSON file: %s\n", filename) ;
    free(filename) ;
    return 1 ;
  }

  FileSystemNode * fs = NULL ;
  json_to_ds(fs_json, fs) ;
  print_fs(fs, 0) ;
	
  // print_json(fs_json) ;
	json_object_put(fs_json) ;

	return fuse_main(argc, argv, &fuse_example_operations, NULL) ;
}
