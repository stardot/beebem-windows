//
// BeebEm Disc Edit - file import/export
//
// Mike Wyatt - Mar 2009
//

#ifndef _DISCEDIT_H
#define _DISCEDIT_H

#define DFS_MAX_CAT_SIZE        62   // Allow for Watford DFS
#define DFS_MAX_TITLE_LEN       12
#define DFS_MAX_NAME_LEN         7
#define DFS_SECTORS_PER_TRACK   10
#define DFS_SECTOR_SIZE        256

typedef struct
{
	char filename[DFS_MAX_NAME_LEN+1];
	char directory;
	bool locked;
	int loadAddr;
	int execAddr;
	int length;
	int startSector;
} DFS_FILE_ATTR;

typedef struct
{
	char title[DFS_MAX_TITLE_LEN+1];
	int numWrites;
	int numFiles;
	int numSectors;
	int bootOpts;
	bool watford62;
	DFS_FILE_ATTR fileAttrs[DFS_MAX_CAT_SIZE];
} DFS_DISC_CATALOGUE;


bool dfs_get_catalogue(const char *szDiscFile, int numSides, int side, DFS_DISC_CATALOGUE *dfsCat);
bool dfs_export_file(const char *szDiscFile, int numSides, int side, DFS_DISC_CATALOGUE *dfsCat,
					 int fileIndex, const char *szExportFolder, char *szErrStr);
bool dfs_import_file(const char *szDiscFile, int numSides, int side, DFS_DISC_CATALOGUE *dfsCat,
					 const char *szFile, const char *szImportFolder, char *szErrStr);

#endif
