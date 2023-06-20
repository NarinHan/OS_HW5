/* taken from https://github.com/fntlnz/fuse-example, and modified */


#define FUSE_USE_VERSION 26

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fuse.h>
#include <json.h>
#include <errno.h>
#include <unistd.h>

#define MAX_NAME_LENGTH 50
#define MAX_DATA_LENGTH 4098

typedef enum _FileType {
  DIRECTORY,
  REGULAR_FILE 
} FileType ;

typedef struct _FileSystemNode {
  int inode ;
  FileType fileType ;
  char name[MAX_NAME_LENGTH] ;
  char data[MAX_DATA_LENGTH] ;
  struct _FileSystemNode * parent ;
  struct _FileSystemNode * firstChild ;
  struct _FileSystemNode * nextSibling ;
  int visited ;
} FileSystemNode ;

FileSystemNode* createNode(int inode, const char* name, FileType fileType, const char* data) {
    FileSystemNode* newNode = (FileSystemNode*)malloc(sizeof(FileSystemNode));
    newNode->inode = inode;
    strncpy(newNode->name, name, MAX_NAME_LENGTH);
    newNode->fileType = fileType;
    strncpy(newNode->data, data, MAX_DATA_LENGTH);
    newNode->parent = NULL;
    newNode->firstChild = NULL;
    newNode->nextSibling = NULL;
    newNode->visited = 0; // Initialize visited flag to 0
    return newNode;
}

void addChild(FileSystemNode* parent, FileSystemNode* child) {
    child->parent = parent;
    if (parent->firstChild == NULL) {
        parent->firstChild = child;
    } else {
        FileSystemNode* sibling = parent->firstChild;
        while (sibling->nextSibling != NULL) {
            sibling = sibling->nextSibling;
        }
        sibling->nextSibling = child;
    }
}

FileSystemNode* parseFileSystem(json_object* json) {
    if (json == NULL || !json_object_is_type(json, json_type_array)) {
        return NULL;
    }

    const int numNodes = json_object_array_length(json);
    FileSystemNode** nodeMap = (FileSystemNode**)malloc(numNodes * sizeof(FileSystemNode*));

    for (int i = 0; i < numNodes; i++) {
        json_object* nodeJson = json_object_array_get_idx(json, i);
        if (nodeJson == NULL || !json_object_is_type(nodeJson, json_type_object)) {
            continue;
        }

        json_object* inodeJson = json_object_object_get(nodeJson, "inode");
        json_object* typeJson = json_object_object_get(nodeJson, "type");

        if (inodeJson == NULL || typeJson == NULL) {
            continue;
        }

        const int inode = json_object_get_int(inodeJson);
        const char* typeStr = json_object_get_string(typeJson);

        FileType fileType;
        if (strcmp(typeStr, "dir") == 0) {
            fileType = DIRECTORY;
        } else if (strcmp(typeStr, "reg") == 0) {
            fileType = REGULAR_FILE;
        } else {
            continue;
        }

        FileSystemNode* node = createNode(inode, "", fileType, "");

        if (fileType == REGULAR_FILE) {
            json_object* dataJson = json_object_object_get(nodeJson, "data");
            if (dataJson != NULL && json_object_is_type(dataJson, json_type_string)) {
                strncpy(node->data, json_object_get_string(dataJson), MAX_DATA_LENGTH);
            }
        }

        nodeMap[inode] = node;
    }

    for (int i = 0; i < numNodes; i++) {
        FileSystemNode* node = nodeMap[i];
        if (node == NULL) {
            continue;
        }

        json_object* entriesJson = json_object_object_get(json_object_array_get_idx(json, i), "entries");
        if (entriesJson == NULL || !json_object_is_type(entriesJson, json_type_array)) {
            continue;
        }

        const int numEntries = json_object_array_length(entriesJson);
        for (int j = 0; j < numEntries; j++) {
            json_object* entryJson = json_object_array_get_idx(entriesJson, j);
            if (entryJson == NULL || !json_object_is_type(entryJson, json_type_object)) {
                continue;
            }

            json_object* nameJson = json_object_object_get(entryJson, "name");
            json_object* inodeJson = json_object_object_get(entryJson, "inode");

            if (nameJson == NULL || inodeJson == NULL) {
                continue;
            }

            const char* name = json_object_get_string(nameJson);
            const int inode = json_object_get_int(inodeJson);

            FileSystemNode* child = nodeMap[inode];
            if (child != NULL) {
                strncpy(child->name, name, MAX_NAME_LENGTH);
                // Prevent circular references
                if (!child->visited && child != node) {
                    addChild(node, child);
                    child->visited = 1; // Mark as visited
                }
            }
        }
    }

    FileSystemNode* root = nodeMap[0];
    free(nodeMap);

    return root;
}

// Display the file system tree recursively
void displayFileSystem(FileSystemNode* node, int depth) {
    int i;
    for (i = 0; i < depth; i++) {
        printf("  ");
    }
    printf("|__%s\n", node->name);

    if (node->fileType == DIRECTORY) {
        FileSystemNode* child = node->firstChild;
        while (child != NULL) {
            displayFileSystem(child, depth + 1);
            child = child->nextSibling;
        }
    }
}

void freeFileSystem(FileSystemNode* node) {
    if (node != NULL) {
        if (node->fileType == DIRECTORY) {
            FileSystemNode* child = node->firstChild;
            while (child != NULL) {
                FileSystemNode* nextSibling = child->nextSibling;
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

  FileSystemNode * root = parseFileSystem(fs_json) ;
	
  // print_json(fs_json) ;
	json_object_put(fs_json) ;

  displayFileSystem(root, 0) ;

  // freeFileSystem(root) ;

	return fuse_main(argc, argv, &fuse_example_operations, NULL) ;
}
