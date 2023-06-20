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
  struct _FileSystemNode * parent ;
  struct _FileSystemNode * firstChild ;
  struct _FileSystemNode * nextSibling ;
  int visited ;
} FileSystemNode ;

FileSystemNode * createNode (int inode, FileType type, char * name, char * data) 
{
  FileSystemNode * newNode = (FileSystemNode *)malloc(sizeof(FileSystemNode)) ;
  
  if (newNode != NULL) {
    newNode->inode = inode ;
    newNode->type = type ;
    // memcpy(newNode->name, name, strlen(name) + 1) ;
    strncpy(newNode->name, name, sizeof(name)) ;
    if (type == REGULAR_FILE) {
      // memcpy(newNode->data, data, strlen(data) + 1) ;
      strncpy(newNode->data, data, sizeof(data)) ;
    } else {
      newNode->data[0] = "\0" ;
    }
    newNode->parent = NULL ;
    newNode->firstChild = NULL ;
    newNode->nextSibling = NULL ;
    newNode->visited = 0 ;
  }

  return newNode ;
}

void addChild (FileSystemNode * parent, FileSystemNode * child) 
{
  if (parent == NULL || child == NULL) {
    return ;
  }

  child->parent = parent ;
  if (parent->firstChild == NULL) {
    parent->firstChild = child ;
  } else {
    FileSystemNode * sibling = parent->firstChild ;
    while (sibling->nextSibling != NULL) {
      sibling = sibling->nextSibling ;
    }
    sibling->nextSibling = child ;
  }
}

FileSystemNode * findNode (FileSystemNode * node, int inode) 
{
  if (node == NULL) {
    return NULL ;
  }

  if (node->inode == inode) {
    return node ;
  }

  FileSystemNode * child = node->firstChild ;
  while (child != NULL) {
    FileSystemNode * result = findNode(child, inode) ;
    if (result != NULL) {
      return result ;
    }
    child = child->nextSibling ;
  }

  return NULL ;
}

// FileType getType (struct json_object * json)
// {
  
// }

FileSystemNode * json_to_ds (struct json_object * json) 
{
  int n = json_object_array_length(json);
  FileSystemNode ** nodeMap = (FileSystemNode **)malloc(n * sizeof(FileSystemNode *)) ;

  // create nodes
	for (int i = 0 ; i < n ; i++) {
		struct json_object * nodeJson = json_object_array_get_idx(json, i);
    int inode ;
    FileType type ;
    char name[MAX_DATA_LENGTH] = "" ;
    char data[MAX_DATA_LENGTH] = "" ;

    json_object * inodeJson = NULL ;
    json_object * typeJson = NULL ;
    json_object_obejct_get_ex(nodeJson, "inode", &inodeJson) ;
    json_object_obejct_get_ex(nodeJson, "type", &typeJson) ;

    if (inodeJson == NULL || typeJson == NULL) {
      continue ;
    }

    int inode = json_object_get_int(inodeJson) ;
    char * typeStr = json_object_get_string(typeJson) ;

    FileType type ;
    if (strcmp(typeStr, "dir") == 0) {
      type = DIRECTORY ;
    } else if (strcmp(typeStr, "reg") == 0) {
      type = REGULAR_FILE ;
    } else {
      continue ;
    }

    FileSystemNode * node = createNode(inode, "", type, "") ;

    if (type == REGULAR_FILE) {
      json_object * dataJson = NULL ;
      json_object_object_get_ex(nodeJson, "data", &dataJson) ;
      if (dataJson != NULL && json_object_is_type(dataJson, json_type_string)) {
        // memcpy(node->data, json_object_get_string(dataJson), strlen(json_object_get_string(dataJson)) + 1) ;
        char * dataStr = json_object_get_string(dataJson) ;
        strncpy(node->data, dataStr, sizeof(dataStr)) ;
      }
    }

    nodeMap[inode] = node ;
  }

  // build tree structure
  for (int i = 0; i < n; i++) {
    FileSystemNode * node = nodeMap[i] ;

    if (node == NULL) {
      continue ;
    }

    json_object * entriesJson = json_object_object_get(json_object_array_get_idx(json, i), "entries") ;
    if (entriesJson == NULL || !json_object_is_type(entriesJson, json_type_array)) {
      continue ;
    }

    int m = json_object_array_length(entriesJson) ;
    for (int j = 0; j < m; j++) {
      json_object * entryJson = json_object_array_get_idx(entryJson, j) ;
      if (entryJson == NULL || !json_object_is_type(entryJson, json_type_object)) {
        continue ;
      }

      json_object* nameJson = json_object_object_get(entryJson, "name");
      json_object* inodeJson = json_object_object_get(entryJson, "inode");
      char * name = json_object_object_ex(entryJson, "name", &nameJson) ;    
      int inode = json_object_object_ex(entryJson, "inode", &inodeJson) ;   


      if (nameJson == NULL || inodeJson == NULL) {
          continue;
      }

      FileSystemNode * child = nodeMap[inode] ;
      if (child != NULL) {
        // memcpy(child->name, name, strlen(name) + 1) ;
        strncpy(child->name, name, sizeof(name)) ;
        if (!child->visited && child != node) { // to prevent circular references
          addChild(node, child) ;
          child->visited = 1 ;
        }
      }
    }
  }

  FileSystemNode * root = nodeMap[0] ;
  free(nodeMap) ;

  return root ;
}

// Display the file system tree recursively
void print_fs (FileSystemNode * node, int depth) {
  for (int i = 0; i < depth; i++) {
      printf("  ");
  }
  printf("|__%s\n", node->name);

  if (node->type == DIRECTORY) {
      FileSystemNode * child = node->firstChild;
      while (child != NULL) {
          print_fs(child, depth + 1);
          child = child->nextSibling;
      }
  }
}

void freeFileSystem (FileSystemNode* node) 
{
  if (node != NULL) {
    if (node->fileType == DIRECTORY) {
        FileSystemNode * child = node->firstChild ;
        while (child != NULL) {
            FileSystemNode * nextSibling = child->nextSibling;
            freeFileSystem(child);
            child = nextSibling;
        }
    }
    free(node);
  }
}

// void print_fs (FileSystemNode * fs, int level)
// {
//   if (fs == NULL)
//     return ;

//   for (int i = 0; i < level; i++) {
//     printf("  ") ;
//   }
//   printf("Inode: %d\n", fs->inode) ;
//   for (int i = 0; i < level; i++) {
//     printf("  ") ;
//   }
//   printf("Name: %s\n", fs->name) ;
//   for (int i = 0; i < level; i++) {
//     printf("  ") ;
//   }
//   printf("Type: %s\n", fs->type == DIRECTORY ? "Directory" : "Regular File") ;
//   if (fs->type == REGULAR_FILE) {
//     for (int i = 0; i < level; i++) {
//       printf("  ") ;
//     }
//     printf("Data: %s\n", fs->data) ;
//   }

// 	// Print the children recursively
//   print_fs(fs->firstChild, level + 1) ;
//   print_fs(fs->nextSibling, level) ;
// }

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




int main(int argc, char *argv[])
{
  // char *filename = NULL ;
  // if (argc < 2) {
  //   printf("Name of a JSON file is required!\n") ;
  //   return 1 ;
  // } else {
  //   filename = (char *)malloc(strlen(argv[1]) + 1) ;
  //   if (filename == NULL) {
  //     printf("Memory allocation failed.\n") ;
  //     return 1 ;
  //   }
  //   memcpy(filename, argv[1], strlen(argv[1]) + 1) ;
  // }

	// struct json_object * fs_json = json_object_from_file(filename) ;
  // if (fs_json == NULL) {
  //   printf("Failed to load JSON file: %s\n", filename) ;
  //   free(filename) ;
  //   return 1 ;
  // }

  struct json_object * fs_json = json_object_from_file("fs.json") ; 

  FileSystemNode * root = json_to_ds(fs_json) ;
  print_fs(root, 0) ;
	
  // print_json(fs_json) ;
	json_object_put(fs_json) ;

  freeFileSystem(root) ;

	return fuse_main(argc, argv, &fuse_example_operations, NULL) ;
}
