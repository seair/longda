/* 程序名:CCfgReader.cpp
 * 作  者:武乃辉
 * 创建时间:2011年2月22日
 * 说  明:读取配置文件,配置文件PARAM=VALUE
 *
 * 修改日志：
 * 修改日期          修改人          修改内容
 * Copyright(c) 2011 Longda Technologies (China), Inc. 
 * All Rights Reserved.
 */

#include <string.h>

#include "defs.h"
#include "cfgreader.h" 

//********************************************************************
CCfgReader::CCfgReader()
{
    Init();
}
//********************************************************************
CCfgReader::~CCfgReader()
{
    Destory();
}
//********************************************************************
void CCfgReader::Init()
{
    try
    {
        m_mapSession.clear();
        InitSessionmap();
    }
    catch(...)
    {
    }
}
void CCfgReader::InitSessionmap()
{
    CONFIG_TYPE_EX * pCfgExt;
    try
    {
        pCfgExt = new CONFIG_TYPE_EX;
        m_pDefaultCfgExt = pCfgExt;
        m_mapSession.insert(CONFIG_TYPE_SESSION_value_type(DEFAULT_SESSION,pCfgExt));
    }
    catch(...)
    {
    }
}
//********************************************************************
void CCfgReader::Destory()
{
    try
    {
        FreeSessionmap();
        m_mapConfig.clear();
    }
    catch(...)
    {
    }
}
void CCfgReader::FreeSessionmap()
{
    CONFIG_TYPE_SESSION::iterator p;
    CONFIG_TYPE_EX * pCfgExt;
    try
    {
        //释放session map
        for(p=m_mapSession.begin();p!=m_mapSession.end();p++)
        {
            //cout<<(*p).first<<" = ["<<(*p).second<<"]"<<endl;
            pCfgExt = (*p).second;
            if(pCfgExt)
            {
                pCfgExt->clear();
                delete pCfgExt;
                pCfgExt = NULL;
            }           
        }
        m_mapSession.clear();
    }
    catch(...)
    {
    }
}
//********************************************************************
int CCfgReader::ReadCfgFile(const string &szCfgFile)
{
    FILE *pf = NULL;
    char szReadBuf[MAX_READCFG_LEN],*pszReadBuf;
    string szLine, szParam, szValue;
    int iPos = 0,i;
    try
    {
        m_mapConfig.clear();
        m_szCfgFile = szCfgFile;
        pf = fopen(m_szCfgFile.c_str(),"r+");
        if(!pf)
        {
            return -1;
        }
        while(!feof(pf))
        {
            pszReadBuf = NULL;
            memset(szReadBuf,0,MAX_READCFG_LEN);
            pszReadBuf = fgets(szReadBuf,MAX_READCFG_LEN,pf);
            if(!pszReadBuf)
            {
                break;
            }
            //测试是否为注释
            for(i=0;i<strlen(szReadBuf);i++)
            {
                if(szReadBuf[i]==' ')
                {
                    continue;
                } 
                else if(szReadBuf[i]=='#')
                {
                    break;
                }
                break;
            }
            if(szReadBuf[i]=='#')
            {
                continue;
            }
            szLine = szReadBuf;
            iPos = szLine.find_first_of('=');
            if(iPos==string::npos)
            {
                continue;
            }
            szParam = szLine.substr(0,iPos);
            szValue = szLine.substr(iPos+1, szLine.size() - iPos - 1);
            iPos = szValue.length()-1;
            if(szValue[iPos] == '\n')
            {
                szValue.erase(iPos);
            }
            trim(szParam);
            trim(szValue);
            m_mapConfig.insert(CONFIG_TYPE_value_type(szParam,szValue));

        }
        fclose(pf);
    }
    catch(...)
    {
        return -1;
    }
    return 0;
}
//********************************************************************
int CCfgReader::ReadCfgFileEx(const string &szCfgFile)
{
    FILE *pf = NULL;
    char szReadBuf[MAX_READCFG_LEN], *pszReadBuf;
    string szLine,szParam,szValue,szSession;
    CONFIG_TYPE_EX * pCfgExt=NULL;
    CONFIG_TYPE_SESSION::iterator p;
    int iPos = 0,i, conLastLine=0;
    try
    {
        m_szCfgFile = szCfgFile;
        pf = fopen(m_szCfgFile.c_str(),"r+");
        if(!pf)
        {
            return -1;
        }
        FreeSessionmap();
        InitSessionmap();
        szSession = DEFAULT_SESSION;
        pCfgExt=m_pDefaultCfgExt;
        while(!feof(pf))
        {
            pszReadBuf = NULL;
            memset(szReadBuf,0,MAX_READCFG_LEN);
            pszReadBuf = fgets(szReadBuf,MAX_READCFG_LEN,pf);
            if(!pszReadBuf)
            {
                break;
            }
            szLine = pszReadBuf;
            trim(szLine);
            if(conLastLine==0)
            {               
                if(szLine[0]=='['&&szLine[szLine.length()-1]==']')
                {//session
                    szSession = szLine.substr(1,szLine.length()-2);
                    p = m_mapSession.find(szSession);
                    if(p != m_mapSession.end())
                    {
                        pCfgExt = (*p).second;
                    }
                    else
                    {
                        pCfgExt = new CONFIG_TYPE_EX;
                        m_mapSession.insert(CONFIG_TYPE_SESSION_value_type(szSession,pCfgExt));
                    }
                    continue;
                }
                //测试是否为注释
                if(szLine[0]=='#')
                {
                    continue;
                }
                
                iPos = szLine.find_first_of('=');
                if(iPos==string::npos)
                {
                    continue;
                }
                szParam = szLine.substr(0,iPos);
                szValue = szLine.substr(iPos+1,szLine.size()-iPos-1);
                iPos = szValue.length()-1;
                if(szValue[iPos] == '\n')
                {
                    szValue.erase(iPos);
                }
                trim(szParam);
                trim(szValue);
                if(szValue[szValue.length()-1]=='\\')
                {
                    szValue = szValue.substr(0,szValue.length()-1);
                    conLastLine = 1;
                    continue;
                }
                szValue = ReplaceValue(szValue);
                pCfgExt->insert(CONFIG_TYPE_EX_value_type(szParam,szValue));
                szValue = "";
            }
            else if(conLastLine==1)
            {
                
                if(szLine[szLine.length()-1]=='\\')
                {
                    szLine = szLine.substr(0,szLine.length()-1);
                }
                else
                {
                    conLastLine = 0;
                }
                szValue = szValue+szLine;
                if(conLastLine=0)
                {
                    szValue = ReplaceValue(szValue);
                    pCfgExt->insert(CONFIG_TYPE_EX_value_type(szParam,szValue));
                    szValue = "";
                }
            }
        }
        fclose(pf);
    }
    catch(...)
    {
        return -1;
    }
    return 0;
}
//********************************************************************
string CCfgReader::GetParamValue(string szParamName)
{
    string szValue = "";
    CONFIG_TYPE::iterator p;
    try
    {
        if(m_mapConfig.size()<=0)
        {
            return szValue;
        }
        p = m_mapConfig.find(szParamName);
        if(p != m_mapConfig.end())
        {
            szValue = (*p).second;
        }
        
    }
    catch(...)
    {
    }
    return szValue;
}
//********************************************************************
string CCfgReader::GetParamValue(string szParamName,string szSession)
{
    string szValue = "";
    CONFIG_TYPE_EX * pCfgExt=NULL;
    CONFIG_TYPE_SESSION::iterator p;
    CONFIG_TYPE_EX::iterator p1;
    try
    {
        if(szSession=="")
        {
            szSession = DEFAULT_SESSION;
        }
        p = m_mapSession.find(szSession);
        if(p != m_mapSession.end())
        {
            pCfgExt = (*p).second;
            p1 = pCfgExt->find(szParamName);
            if(p1 != pCfgExt->end())
            {
                szValue = (*p1).second;
            }
        }
    }
    catch(...)
    {
    }
    return szValue;
}
//********************************************************************
void CCfgReader::trim(string & str)
{
    int i,length;
    string sztemp = "";
    try
    {
        length = str.length();
        for(i=0;i<length;i++)
        {
            if(str[i]==' '||str[i]=='\t'||str[i]=='\n')
            {
                continue;
            }
            break;
        }
        if(i==(length-1)&&(str[i]==' '||str[i]=='\t'||str[i]=='\n'))
        {
            str = "";
            return;
        }
        str = str.substr(i,length-i);
        length = str.length();
        for(i=length-1;i>0;i--)
        {
            if(str[i]==' '||str[i]=='\t'||str[i]=='\n')
            {
                continue;
            }
            break;
        }
        str = str.substr(0,i+1);
    }
    catch(...)
    {
    }
}
//********************************************************************
void CCfgReader::PrintParam()
{
    CONFIG_TYPE::iterator p;
    try
    {
        for(p=m_mapConfig.begin();p!=m_mapConfig.end();p++)
        {
            cout<<(*p).first<<" = ["<<(*p).second<<"]"<<endl;
        }
    }
    catch(...)
    {
    }
}
//********************************************************************
string CCfgReader::ReplaceString(string str,string replace,string value)
{
    string szRt = "",szTemp = "";
    int index;
    try
    {
        szRt = str;
        if(replace.length()>str.length()||replace==""||str=="")
        {
            return szRt;
        }
        index = szRt.find (replace);
        while(index!=string::npos)
        {
            szTemp = szTemp+szRt.substr(0,index);
            szTemp = szTemp+value;
            szRt = szRt.substr(index+replace.length(),szRt.length()-index-replace.length());
            index = szRt.find (replace);
        }
        if(szTemp=="")
        {
            return szRt;
        }
        if(szRt!="")
        {
            szTemp = szTemp+szRt;
        }
        szRt = szTemp;
    }
    catch(...)
    {
    }
    return szRt;
}
//********************************************************************
string CCfgReader::ReplaceValue(string szValue)
{
    string szRt = "";
    try
    {
        szRt = szValue;
        szRt = ReplaceString(szRt,"\\n","\n");
        szRt = ReplaceString(szRt,"\\t","\t");
    }
    catch(...)
    {
        szRt = szValue;
    }
    return szRt;
}
