// __CR__
// Copyright (c) 2008-2012 LongdaFeng
// All Rights Reserved
// 
// This software contains the intellectual property of LongdaFeng
// or is licensed to LongdaFeng from third parties.  Use of this 
// software and the intellectual property contained therein is 
// expressly limited to the terms and conditions of the License Agreement  
// under which it is provided by or on behalf of LongdaFeng.
// __CR__

/*
 * io.h
 *
 *  Created on: Mar 26, 2012
 *      Author: Longda Feng
 */

#ifndef IO_H_
#define IO_H_



#include <string>
#include <vector>

#include "defs.h"

static const char FILE_PATH_SPLIT = '/';
static const char FILE_PATH_SPLIT_STR[] = "/";

/**
 * read data from file fileName, store data to data
 * if success, store file continent to data
 * if fail, return -1 and don't change data
 */
int readFromFile(const std::string &fileName, char *&data, size_t &fileSize);

int writeToFile(const std::string &fileName, const char *data, u32_t dataSize, const char *openMode);

/**
 * return the line number which line.strip() isn't empty
 */
int getFileLines(const std::string &fileName, u64_t &lineNum);


/** Get file list from the dir
 * don't care ".", "..", ".****" hidden files
 * just count regular files, don't care directory
 * @param[out]  fileList   file List
 * @param[in]   path       the search path
 * @param[in]   pattern    regex string, if not empty, the file should match list
 * @param[in]   resursion  if this has been set, it will search subdirs
 * @return  0   if success, error code otherwise
 */
int getFileList(std::vector<std::string> &fileList, const std::string &path,
        const std::string &pattern, bool resusion);
int getFileNum(u64_t &fileNum, const std::string &path,
        const std::string &pattern, bool resusion);
int getDirList(std::vector<std::string> &dirList, const std::string &path,
        const std::string &pattern);

/**
 * extract file name/file director from the fullPath
 * if  the last character is '/' such as "/" or "/tmp/", it will return ""
 */
std::string getFileName(const std::string &fullPath);
void        getFileName(const char *path, std::string &fileName);

std::string getDirName(const std::string &fullPath);
void        getDirName(const char *path, std::string &parent);

std::string getAboslutPath(const char *path);

int touch(const std::string &fileName);

/**
 * get file size
 */
int getFileSize(const char *filePath, u64_t &fileLen);




#endif /* IO_H_ */
