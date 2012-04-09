#if !defined(INCLUDE_CFGREADER_H)
#define INCLUDE_CFGREADER_H

#include <string>
#include <map>
#include <iostream>
#include <stdio.h>
using namespace std;

#define CFG_DELIMIT_TAG             ","
#define CFG_DELIMIT_TAG_C           ','

//********************************************************************
//配置文件读取器
//配置文件格式
//VARNAME=VALUE ,#开头行表示注释行
typedef map<string, string, less<string> > CONFIG_TYPE;
typedef CONFIG_TYPE::value_type CONFIG_TYPE_value_type;

//ReadCfgFileEx
//支持session配置
typedef map<string, string, less<string> > CONFIG_TYPE_EX,*PCONFIG_TYPE_EX;
typedef CONFIG_TYPE_EX::value_type CONFIG_TYPE_EX_value_type;
    
typedef map<string, PCONFIG_TYPE_EX, less<string> > CONFIG_TYPE_SESSION;    
typedef CONFIG_TYPE_SESSION::value_type CONFIG_TYPE_SESSION_value_type;

    
#define MAX_READCFG_LEN 1024
#define DEFAULT_SESSION "DEFAULT"
class CCfgReader
{
private:
    string m_szCfgFile;
    CONFIG_TYPE m_mapConfig;
    CONFIG_TYPE_SESSION m_mapSession;
    CONFIG_TYPE_EX * m_pDefaultCfgExt;
public:
    CCfgReader();
    ~CCfgReader();
    void Init();
    void InitSessionmap();
    void Destory();
    void FreeSessionmap();
    //0 means success, others means failure
    int ReadCfgFile(const string &szCfgFile);
	//0 means success, others means failure
	//Support session management
    int ReadCfgFileEx(const string &szCfgFile);
    //取参数值
    string GetParamValue(string szParamName);
    //取参数值
    string GetParamValue(string szParamName,string szSession=DEFAULT_SESSION);
    //左右去空格
    void trim(string & str);
    //
    void PrintParam();
    string ReplaceString(string str,string replace,string value);
    string ReplaceValue(string szValue);
};
//********************************************************************


#endif

