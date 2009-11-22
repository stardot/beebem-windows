/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2009  Mike Wyatt

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public 
License along with this program; if not, write to the Free 
Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
Boston, MA  02110-1301, USA.
****************************************************************/

//
// BeebEm Disc Edit - file import/export
//
// Mike Wyatt - Mar 2009
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "discedit.h"

#define DFS_LENGTH_TO_SECTORS(l) (((int)l + DFS_SECTOR_SIZE - 1) / DFS_SECTOR_SIZE)

static void strip_trailing_spaces(char *str)
{
	size_t l = strlen(str);
	while (l > 0 && str[l - 1] == ' ')
	{
		str[l - 1] = 0;
		l--;
	}
}

static void dfs_get_files_from_cat(
	unsigned char *sect0, unsigned char *sect1, int numFiles, DFS_FILE_ATTR *attrs)
{
	int i;
	int offset;
	unsigned char c;

	for (i = 0; i < numFiles; ++i)
	{
		// Reverse order so files are in start sector order
		offset = (numFiles - i) * 8;
		memcpy(attrs[i].filename, sect0 + offset, 7);
		strip_trailing_spaces(attrs[i].filename);
		attrs[i].directory = sect0[offset + 7] & 0x7f;
		attrs[i].locked = (sect0[offset + 7] & 0x80) ? true : false;
		c = sect1[offset + 6];
		attrs[i].loadAddr = sect1[offset] + (sect1[offset + 1] << 8) + ((c & 0x0c) ? 0xffff0000 : 0);
		attrs[i].execAddr = sect1[offset + 2] + (sect1[offset + 3] << 8) + ((c & 0xc0) ? 0xffff0000 : 0);
		attrs[i].length = sect1[offset + 4] + (sect1[offset + 5] << 8) + ((c & 0x30) << 12);
		attrs[i].startSector = sect1[offset + 7] + ((c & 0x03) << 8);
	}
}

bool dfs_get_catalogue(
	const char *szDiscFile, int numSides, int side, DFS_DISC_CATALOGUE *dfsCat)
{
	bool success = true;
	FILE *fd;
	int catOffset = 0;
	unsigned char buffer[DFS_SECTOR_SIZE * 4];
	int i;

	fd = fopen(szDiscFile, "rb");
	if (fd == NULL)
	{
		success = false;
	}
	else
	{
		catOffset = 0;
		if (numSides == 2 && side == 1)
		{
			// Tracks are interleaved in a double sided disc image
			catOffset = DFS_SECTORS_PER_TRACK * DFS_SECTOR_SIZE;
		}

		if (fseek(fd, catOffset, SEEK_SET) != 0)
		{
			success = false;
		}
		else
		{
			memset(buffer, 0, DFS_SECTOR_SIZE * 4);
			if (fread(buffer, 1, DFS_SECTOR_SIZE * 4, fd) < DFS_SECTOR_SIZE * 2)
			{
				success = false;
			}
			else
			{
				memset(dfsCat, 0, sizeof(DFS_DISC_CATALOGUE));
				memcpy(dfsCat->title, buffer, 8);
				memcpy(dfsCat->title + 8, buffer + DFS_SECTOR_SIZE, 4);
				strip_trailing_spaces(dfsCat->title);
				dfsCat->numWrites = buffer[DFS_SECTOR_SIZE + 4];
				dfsCat->numFiles = buffer[DFS_SECTOR_SIZE + 5] / 8;
				dfsCat->numSectors = ((buffer[DFS_SECTOR_SIZE + 6] & 3) << 8) + buffer[DFS_SECTOR_SIZE + 7];
				dfsCat->bootOpts = (buffer[DFS_SECTOR_SIZE + 6] >> 4) & 3;
			
				// Read first 31 files
				dfs_get_files_from_cat(
					buffer, buffer + DFS_SECTOR_SIZE, dfsCat->numFiles, dfsCat->fileAttrs);

				// Check for a Watford DFS 62 file catalogue
				for (i = DFS_SECTOR_SIZE * 2; i < DFS_SECTOR_SIZE * 2 + 8; ++i)
				{
					if (buffer[i] != 0xAA)
						break;
				}
				if (i == DFS_SECTOR_SIZE * 2 + 8)
				{
					dfsCat->watford62 = true;

					// Total number of sectors is in second catalogue
					dfsCat->numSectors = ((buffer[DFS_SECTOR_SIZE * 3 + 6] & 3) << 8) +
						buffer[DFS_SECTOR_SIZE * 3 + 7];

					i = buffer[DFS_SECTOR_SIZE * 3 + 5] / 8;
					dfs_get_files_from_cat(
						buffer + DFS_SECTOR_SIZE * 2, buffer + DFS_SECTOR_SIZE * 3,
						i, &dfsCat->fileAttrs[dfsCat->numFiles]);
					dfsCat->numFiles += i;
				}
			}
		}

		fclose(fd);
	}

	return success;
}

bool dfs_export_file(const char *szDiscFile, int numSides, int side, DFS_DISC_CATALOGUE *dfsCat,
					 int fileIndex, const char *szExportFolder, char *szErrStr)
{
	bool success = true;
	DFS_FILE_ATTR *attr;
	char filename[_MAX_PATH];
	FILE *discfd;
	FILE *filefd;
	int offset;
	int len;
	int sector;
	int track;
	int n;
	unsigned char buffer[DFS_SECTOR_SIZE];

	attr = &dfsCat->fileAttrs[fileIndex];

	discfd = fopen(szDiscFile, "rb");
	if (discfd == NULL)
	{
		sprintf(szErrStr, "Failed to open disc file:\n  %s", szDiscFile);
		return false;
	}

	// Build file name
	memset(filename, 0, _MAX_PATH);
	strcpy(filename, szExportFolder);
	strcat(filename, "/");
	filename[strlen(filename)] = attr->directory;
	strcat(filename, ".");
	strcat(filename, attr->filename);

	filefd = fopen(filename, "wb");
	if (filefd == NULL)
	{
		sprintf(szErrStr, "Failed to open file for writing:\n  %s", filename);
		success = false;
	}
	else
	{
		// Export the data
		sector = attr->startSector;
		len = attr->length;
		while (success && len > 0)
		{
			// Calc file offset for next sector
			if (numSides == 1)
			{
				offset = sector * DFS_SECTOR_SIZE;
			}
			else
			{
				track = sector / DFS_SECTORS_PER_TRACK;
				offset = (track * 2 + side) * DFS_SECTORS_PER_TRACK;
				offset += (sector % DFS_SECTORS_PER_TRACK);
				offset *= DFS_SECTOR_SIZE;
			}

			// Read next sector
			n = len < DFS_SECTOR_SIZE ? len : DFS_SECTOR_SIZE;
			if (fseek(discfd, offset, SEEK_SET) != 0 ||
				fread(buffer, 1, n, discfd) != n)
			{
				sprintf(szErrStr, "Failed to read data from:\n  %s", szDiscFile);
				success = false;
			}
			else
			{
				// Export to file
				if (fwrite(buffer, 1, n, filefd) != n)
				{
					sprintf(szErrStr, "Failed to write data to:\n  %s", filename);
					success = false;
				}
			}

			sector++;
			len -= n;
		}

		fclose(filefd);

		if (success)
		{
			// Create INF file
			strcat(filename, ".INF");
			filefd = fopen(filename, "wb");
			if (filefd == NULL)
			{
				sprintf(szErrStr, "Failed to open file for writing:\n  %s", filename);
				success = false;
			}
			else
			{
				fprintf(filefd, "%c.%-7s %06X %06X %06X\n",
						attr->directory,
						attr->filename,
						attr->loadAddr & 0xffffff,
						attr->execAddr & 0xffffff,
						attr->length);
				fclose(filefd);
			}
		}
	}

	fclose(discfd);

	return success;
}

static void dfs_write_files_to_cat(
	unsigned char *sect0, unsigned char *sect1, int numFiles, DFS_FILE_ATTR *attrs)
{
	int i, j;
	int offset;
	unsigned char c;

	for (i = 0; i < numFiles; ++i)
	{
		// Reverse order so files are in reverse start sector order
		offset = (numFiles - i) * 8;

		memcpy(sect0 + offset, attrs[i].filename, 7);
		for (j = (int)strlen(attrs[i].filename); j < 7; ++j)
			sect0[offset + j] = ' ';

		sect0[offset + 7] = attrs[i].directory | (attrs[i].locked ? 0x80 : 0);

		c = 0;
		sect1[offset] = attrs[i].loadAddr & 0xff;
		sect1[offset + 1] = (attrs[i].loadAddr >> 8) & 0xff;
		c |= ((attrs[i].loadAddr >> 16) & 0x3) << 2;

		sect1[offset + 2] = attrs[i].execAddr & 0xff;
		sect1[offset + 3] = (attrs[i].execAddr >> 8) & 0xff;
		c |= ((attrs[i].execAddr >> 16) & 0x3) << 6;

		sect1[offset + 4] = attrs[i].length & 0xff;
		sect1[offset + 5] = (attrs[i].length >> 8) & 0xff;
		c |= ((attrs[i].length >> 16) & 0x3) << 4;

		sect1[offset + 7] = attrs[i].startSector & 0xff;
		c |= ((attrs[i].startSector >> 8) & 0x3);

		sect1[offset + 6] = c;
	}
}

bool dfs_import_file(const char *szDiscFile, int numSides, int side, DFS_DISC_CATALOGUE *dfsCat,
					 const char *szFile, const char *szImportFolder, char *szErrStr)
{
	bool success = true;
	DFS_FILE_ATTR *attrs;
	char filename[_MAX_PATH];
	char infname[_MAX_PATH];
	char dfsname[DFS_MAX_NAME_LEN + 3];
	FILE *discfd;
	FILE *filefd;
	int loadAddr;
	int execAddr;
	int fileLen;
	int startSector;
	int catIndex;
	int numSectors;
	int sector;
	int len;
	int offset;
	int track;
	int i, j, n;
	unsigned char buffer[DFS_SECTOR_SIZE * 4];

	attrs = dfsCat->fileAttrs;

	// Open the DFS disc
	discfd = fopen(szDiscFile, "rb+");
	if (discfd == NULL)
	{
		sprintf(szErrStr, "Failed to open disc file for writing:\n  %s", szDiscFile);
		return false;
	}

	// Build file names
	strcpy(filename, szImportFolder);
	strcat(filename, "/");
	strcat(filename, szFile);
	strcpy(infname, filename);
	strcat(infname, ".INF");

	// Read .INF file
	loadAddr = 0;
	execAddr = 0;
	fileLen = -1;
	filefd = fopen(infname, "rb");
	if (filefd != NULL)
	{
		if (fscanf(filefd, "%9s %X %X", dfsname, &loadAddr, &execAddr) < 1)
		{
			sprintf(szErrStr, "Failed to read file attributes from:\n  %s", infname);
			success = false;
		}
		fclose(filefd);
	}
	else
	{
		// No .INF file, construct dfsname
		memset(dfsname, 0, sizeof(dfsname));
		if (strlen(szFile) >= 3 && szFile[1] == '.')
		{
			dfsname[0] = szFile[0];
			dfsname[1] = '.';
			j = 2;
		}
		else
		{
			strcpy(dfsname, "$.");
			j = 0;
		}
		i = 0;
		while (i < DFS_MAX_NAME_LEN && szFile[j] != 0)
		{
			char c = toupper(szFile[j]);
			if (c >= 'A' && c <= 'Z' ||
				c >= '0' && c <= '9' ||
				c == '!' || c == '$' || c == '%' || c == '^' || c == '&' || c == '(' || c == ')' ||
				c == '_' || c == '-' || c == '=' || c == '+' || c == '[' || c == ']' || c == '{' ||
				c == '}' || c == '@' || c == '#' || c == '~' || c == ',')
			{
				dfsname[i+2] = c;
				i++;
			}
			j++;
		}
		if (i == 0)
		{
			sprintf(szErrStr, "Failed to create DFS file name for:\n  %s", szFile);
			success = false;
		}
	}

	if (success)
	{
		// Determine file len?
		if (fileLen == -1)
		{
			filefd = fopen(filename, "rb");
			if (filefd == NULL)
			{
				sprintf(szErrStr, "Failed to open file:\n  %s", filename);
				success = false;
			}
			else
			{
				fseek(filefd, 0, SEEK_END);
				fileLen = ftell(filefd);
				fclose(filefd);
			}
		}

		if (fileLen >= 0x80000)
		{
			sprintf(szErrStr, "File too large to import:\n  %s", filename);
			success = false;
		}
	}

	if (success)
	{
		// Search catalogue for existing file with same name
		for (i = 0; i < dfsCat->numFiles; ++i)
		{
			if (attrs[i].directory == dfsname[0] &&
				strcmp(attrs[i].filename, dfsname + 2) == 0)
			{
				// Delete the file
				memmove(&attrs[i], &attrs[i+1],
						sizeof(DFS_FILE_ATTR) * dfsCat->numFiles - i - 1);
				dfsCat->numFiles--;
				break;
			}
		}
	}

	if (success)
	{
		// Check for space in the catalogue
		if (dfsCat->watford62 && dfsCat->numFiles >= 62 ||
			!dfsCat->watford62 && dfsCat->numFiles >= 31)
		{
			sprintf(szErrStr, "Catalogue full, cannot import %s", szFile);
			success = false;
		}
	}

	if (success)
	{
		// Search catalogue for suitable gap (catalogue is in startSector order)
		if (dfsCat->watford62)
			startSector = 4;
		else
			startSector = 2;

		numSectors = DFS_LENGTH_TO_SECTORS(fileLen);
		for (i = 0; i < dfsCat->numFiles; ++i)
		{
			if ((attrs[i].startSector - startSector) >= numSectors)
			{
				// Found a gap large enough
				break;
			}
			else
			{
				startSector = attrs[i].startSector + DFS_LENGTH_TO_SECTORS(attrs[i].length);
			}
		}
		if (i == dfsCat->numFiles)
		{
			// Space at end of disc?
			if ((dfsCat->numSectors - startSector) < numSectors)
			{
				sprintf(szErrStr, "Insufficient space to import %s", szFile);
				success = false;
			}
		}
		catIndex = i;
	}

	if (success)
	{
		// Import the data
		filefd = fopen(filename, "rb");
		if (filefd == NULL)
		{
			sprintf(szErrStr, "Failed to open file:\n  %s", filename);
			success = false;
		}
		else
		{
			sector = startSector;
			len = fileLen;
			while (success && len > 0)
			{
				// Calc file offset for next sector
				if (numSides == 1)
				{
					offset = sector * DFS_SECTOR_SIZE;
				}
				else
				{
					track = sector / DFS_SECTORS_PER_TRACK;
					offset = (track * 2 + side) * DFS_SECTORS_PER_TRACK;
					offset += (sector % DFS_SECTORS_PER_TRACK);
					offset *= DFS_SECTOR_SIZE;
				}

				// Read next sector
				memset(buffer, 0, DFS_SECTOR_SIZE);
				n = len < DFS_SECTOR_SIZE ? len : DFS_SECTOR_SIZE;
				if (fread(buffer, 1, n, filefd) != n)
				{
					sprintf(szErrStr, "Failed to read data from:\n  %s", filename);
					success = false;
				}
				else
				{
					// Import to file
					if (fseek(discfd, offset, SEEK_SET) != 0 ||
						fwrite(buffer, 1, DFS_SECTOR_SIZE, discfd) != DFS_SECTOR_SIZE)
					{
						sprintf(szErrStr, "Failed to write data to:\n  %s", szDiscFile);
						success = false;
					}
				}

				sector++;
				len -= n;
			}

			fclose(filefd);
		}
	}

	if (success)
	{
		// Update the catalogue
		offset = 0;
		if (numSides == 2 && side == 1)
		{
			// Tracks are interleaved in a double sided disc image
			offset = DFS_SECTORS_PER_TRACK * DFS_SECTOR_SIZE;
		}
		if (fseek(discfd, offset, SEEK_SET) != 0)
		{
			sprintf(szErrStr, "Failed to seek in file:\n  %s", szDiscFile);
			success = false;
		}
		else
		{
			memset(buffer, 0, DFS_SECTOR_SIZE * 4);
			numSectors = dfsCat->watford62 ? 4 : 2;
			if (fread(buffer, 1, DFS_SECTOR_SIZE * numSectors, discfd) !=
				(DFS_SECTOR_SIZE * numSectors))
			{
				sprintf(szErrStr, "Failed to read catalogue from:\n  %s", szDiscFile);
				success = false;
			}
			else
			{
				// Add file to catalogue
				memmove(&attrs[catIndex+1], &attrs[catIndex],
						sizeof(DFS_FILE_ATTR) * dfsCat->numFiles - catIndex);
				strcpy(attrs[catIndex].filename, dfsname + 2);
				attrs[catIndex].directory = dfsname[0];
				attrs[catIndex].locked = false;
				attrs[catIndex].loadAddr = loadAddr;
				attrs[catIndex].execAddr = execAddr;
				attrs[catIndex].length = fileLen;
				attrs[catIndex].startSector = startSector;
				dfsCat->numFiles++;

				// Update catalogue sectors
				n = (dfsCat->numFiles > 31) ? 31 : dfsCat->numFiles;
				buffer[DFS_SECTOR_SIZE + 5] = n * 8;
				dfs_write_files_to_cat(buffer, buffer + DFS_SECTOR_SIZE, n, attrs);

				if (dfsCat->watford62)
				{
					n = (dfsCat->numFiles > 31) ? dfsCat->numFiles - 31 : 0;
					buffer[DFS_SECTOR_SIZE * 3 + 5] = n * 8;
					dfs_write_files_to_cat(buffer + DFS_SECTOR_SIZE * 2, buffer + DFS_SECTOR_SIZE * 3,
										   n, &attrs[31]);
				}

				// Write catalogue sectors back to file
				if (fseek(discfd, offset, SEEK_SET) != 0)
				{
					sprintf(szErrStr, "Failed to seek in file:\n  %s", szDiscFile);
					success = false;
				}
				else if (fwrite(buffer, 1, DFS_SECTOR_SIZE * numSectors, discfd) !=
						 (DFS_SECTOR_SIZE * numSectors))
				{
					sprintf(szErrStr, "Failed to write catalogue to:\n  %s", szDiscFile);
					success = false;
				}
			}
		}
	}

	fclose(discfd);

	if (!success)
	{
		// Re-read the catalogue from the disc image
		dfs_get_catalogue(szDiscFile, numSides, side, dfsCat);
	}

	return success;
}
