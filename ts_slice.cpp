#include <iostream>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h> 
#include <algorithm> 
#include <math.h>

#include "ts_slice.h"

static int scan_unitStart(std::string file, unsigned short pid, std::vector<long long> &u_list)
{
    printf("\nstart scan_unitStart\n");
    FILE *fp = NULL;
    fp = fopen(file.c_str(), "r");
    if (!fp){
        printf("open file error\n");
        return -1;
    }
    
    const int bufLen = 188*1000;
    long long fileOffset = 0;
    long long readCount = 0;
    unsigned char buf[bufLen] = {0};
    int readLen = 0;
        
    while(1){
        readLen = fread(buf, 1, bufLen, fp);
        if (readLen <= 0){
            //printf("read file finish\n");
            break;
        }
        
        int index = 0;
        while(index + 4 < readLen ){
            if (buf[index] != 0x47){//find sync byte
                index++;
                continue;
            }
            
            unsigned char *ptr = &buf[index];
            
            if( ptr[1]&0x80 ){
                printf("package error\n");
                index += 188;
                continue;
            }
            
            #if 0
            for(int x = 0; x < 4; ++x){
                printf("%02x ", ptr[x]);
            }
            printf("\n");
            #endif

            unsigned short cpid = ptr[1] & 0x1f;
            cpid = cpid<<8 | ptr[2];
            if( pid && pid != cpid ){
                index += 188;
                continue;
            }
            
            unsigned int b_unit_start = ptr[1]&0x40;
            if (b_unit_start == 0x40){
                fileOffset = readCount + index;
                u_list.push_back(fileOffset);
                //printf("%lld\n", fileOffset);
            }
            index += 188;
        }  
        readCount += readLen; 
    }
    
    fclose(fp);   

    printf("end scan_unitStart\n\n");
    
    return 0;
}

static int scan_frameStart(std::string file, unsigned short pid, std::vector<long long> &u_list, std::vector<long long> &f_list)
{
    printf("start scan_frameStart\n");
    
    if (u_list.size() < 1){
        printf("No unit index\n");
        return -1;
    }
    
    FILE *fp = NULL;
    fp = fopen(file.c_str(), "r");
    if (!fp){
        printf("open file error\n");
        return -1;
    }
        
    int readLen = 0;
    int i = 0;
    for(i = 0; i < u_list.size() - 1; ++i){
        
        int bufLen = u_list[i+1] - u_list[i] + 1;
        unsigned char *buf = (unsigned char*)malloc(bufLen);
        unsigned char *pesBuf = (unsigned char*)malloc(bufLen);
        if (!buf){
            printf("malloc error\n");
            break;
        }
        
        fseeko64(fp, u_list[i], SEEK_SET);
        readLen = fread(buf, 1, bufLen, fp);
        if (readLen != bufLen){
            printf("read file error\n");
            break;
        }
        
        int index = 0;
        int pesLen = 0;
        while(index + 4 < readLen ){
            if (buf[index] != 0x47){//find sync byte
                index++;
                continue;
            }
            
            unsigned char *ptr = &buf[index];
            
            if( ptr[1]&0x80 ){
                printf("package error\n");
                index += 188;
                continue;
            }

            unsigned short cpid = ptr[1] & 0x1f;
            cpid = cpid<<8 | (ptr[2]&0xff);
            if( pid && pid != cpid ){
                index += 188;
                continue;
            }
            
            #if 0
            for(int x = 0; x < 11; ++x){
                printf("%02x ", ptr[x]);
            }
            printf("\n");
            #endif
            
            unsigned int b_unit_start = ptr[1]&0x40;
            unsigned int b_adaptation = ptr[3]&0x20;
            
            int pos = 0;
            if( !b_adaptation ){
                pos = 4;
            }
            else{
                pos = 4 + 1 + ptr[4];
            }    
            
            unsigned char *load = ptr + pos;
            int loadLen = 188 - pos;
            memcpy(pesBuf + pesLen, load, loadLen);
            pesLen += loadLen;

            index += 188;
        }
        
        int pesHeadLen = 9 + pesBuf[8];
        
        #if 0
        FILE *fp2 = fopen("./111.es.h264", "a+");
        if (fp2){
            
            fwrite(pesBuf+pesHeadLen, 1, pesLen-pesHeadLen, fp2);
            fclose(fp2);
        }
        //int c = getchar();
        #endif

        unsigned char *p = pesBuf + pesHeadLen;
        int len = pesLen - pesHeadLen;
        while(len > 6){
            if(p[0] == 0x00 && p[1] == 0x00 && p[2] == 0x01){
                unsigned int nal_unit_type = p[3] & 0x1f;
                if(nal_unit_type >=1 && nal_unit_type <= 5 &&  (p[4]&0x80) ){
                    //printf("%02x %02x %02x %02x \n", p[3], p[4], p[5], p[6]);
                    f_list.push_back(u_list[i]);
                    break; //?
                }
            }

            p++;
            len--;        
        }          
        
        free(buf);
        free(pesBuf);
    }
    
    //last piece
    f_list.push_back(u_list[i]);
    
    fclose(fp);   

    printf("end scan_frameStart\n\n");
    
    return 0;
}

static int cut_file(std::string file, std::vector<long long> &f_list, int frameCount, std::string destDir, std::vector<std::string> &name_list)
{
    printf("start cut_file\n");
    
    if (f_list.size() < 1){
        printf("No frame index\n");
        return -1;
    }
    
    FILE *fp = NULL;
    FILE *fp2 = NULL;
    fp = fopen(file.c_str(), "r");
    if (!fp){
        printf("open file error\n");
        return -1;
    }    
    
    char fileName[1024] = {0};
    long long start = 0; 
    int len = 0;
    int i = 0;
    int index = 0;
    for(i = frameCount; i < f_list.size(); i=i+frameCount, index++){
        len = f_list[i] - start;
        printf("Slice start:%lld, len:%d\n", start, len);
        fseeko64(fp, start, SEEK_SET);
        //copy
        snprintf(fileName, sizeof(fileName), "%s/%08d.ts", destDir.c_str(), index);
        name_list.push_back(fileName);
        //printf("file name: %s\n", fileName);
        
        fp2 = fopen(fileName, "w+");
        if (fp2){
            int count = 0;
            while(count < len){
                fputc(fgetc(fp), fp2);
                count++;
            }
            fclose(fp2);
        }else{
		printf("open file error\n");
	}        
    
        start = f_list[i];
    }
    
    //copy tail
    fseeko64(fp, 0, SEEK_END);
    long long filelen = ftello64(fp);
    
    fseeko64(fp, start, SEEK_SET);
    snprintf(fileName, sizeof(fileName), "%s/%08d.ts", destDir.c_str(), index);
    name_list.push_back(fileName);
    fp2 = fopen(fileName, "w+");
    if (fp2){
            int count = 0;
            do{
                fputc(fgetc(fp), fp2);
                count++;
            }while(count < (filelen - start));
        fclose(fp2);
    }
    
    fclose(fp);
    
    printf("end cut_file\n\n");
    
    return 0;
}

static bool sort_by_name(std::string a, std::string b) 
{
    int aa = strtol(a.c_str(), NULL, 10);
    int bb = strtol(b.c_str(), NULL, 10);
    return (aa < bb); 
}  

static int generate_m3u8(std::vector<std::string> &name_list, std::string dir, int frameCount)
{
    printf("start generate_m3u8\n");
    
    float time = (float)frameCount/30;
    
    std::vector<std::string> file_list;
    
#if 0
    DIR *dirptr = opendir(dir.c_str());
    if (dirptr){
        struct dirent *entry = NULL;
        while(entry = readdir(dirptr)){
            if (entry->d_type == 8){
                //printf("entry->d_name:%s\n", entry->d_name);
                std::string name = entry->d_name;
                if(name.length() == 11 && name.find(".ts", 8)){
                    file_list.push_back(name);
                    //printf("%s\n", name.c_str());
                }
            }            
        }
        closedir(dirptr);
    }    
    else{
        printf("open dir error\n");
        return -1;
    }
#else
    for(int i = 0; i < name_list.size(); i++){
        int pos = name_list[i].rfind('/');
        if (std::string::npos != pos){
            std::string name = name_list[i].substr(pos + 1);
            file_list.push_back(name);
            //printf("%s\n", name.c_str());
        }
    }    
#endif
    
    std::sort(file_list.begin(), file_list.end(), sort_by_name);
    
    char buf[1024*1024] = {0};
    int len = 0;
    
    snprintf(buf + len, sizeof(buf)-len, "%s\r\n", "#EXTM3U");
    len = strlen(buf);
    
    snprintf(buf + len, sizeof(buf)-len, "%s%.2f\r\n", "#EXT-X-TARGETDURATION:", ceil(time));
    len = strlen(buf);   
    
    snprintf(buf + len, sizeof(buf)-len, "%s\r\n", "#EXT-X-VERSION:3");
    len = strlen(buf);     
    
    snprintf(buf + len, sizeof(buf)-len, "%s\r\n", "#EXT-X-ALLOW-CACHE:NO");
    len = strlen(buf);  

    snprintf(buf + len, sizeof(buf)-len, "%s\r\n", "#EXT-X-PLAYLIST-TYPE:VOD");
    len = strlen(buf);     
    
    snprintf(buf + len, sizeof(buf)-len, "%s\r\n", "#EXT-X-MEDIA-SEQUENCE:1");
    len = strlen(buf);      
    
    printf("file count : %d\n", file_list.size());
    int file_count = file_list.size();
    int i = 0;
    while(i < file_count){
        snprintf(buf + len, sizeof(buf)-len, "%s%.2f,\r\n", "#EXTINF:", time);
        len = strlen(buf);    

        snprintf(buf + len, sizeof(buf)-len, "%s\r\n", file_list[i].c_str());
        len = strlen(buf);         
        
        i++;
    }
    
    snprintf(buf + len, sizeof(buf)-len, "%s\r\n", "#EXT-X-ENDLIST");
    len = strlen(buf); 
    
    char fileName[1204] = {0};
    snprintf(fileName, sizeof(fileName), "%s/start.m3u8", dir.c_str());
    FILE *fp2 = fopen(fileName, "w+");
    if (fp2){
            fwrite(buf,1,len,fp2);
        fclose(fp2);
    }
    else{
        printf("open file error\n");
        return -1;
    }
    
    printf("end generate_m3u8\n\n");
    
    return 0;
}

int ts_slice(const char *filePath, int frameCount, const char* destDir)
{
    if (!filePath || !destDir || frameCount < 0){
        printf("Input error\n");
        return -1;
    }
    
    printf("source file:[%s]\n", filePath);
    printf("frameCount:[%d]\n", frameCount);
    printf("dest dir:[%s]\n", destDir);
    
    unsigned short pid = 0;
    int ret = 0;
    std::vector<long long> unitStart_list;
    
    ret = scan_unitStart(filePath, pid, unitStart_list);
    if(ret){
        printf("scan_unitStart error\n");
        return -1;
    }
    
    std::vector<long long> frameIndex_list;
    ret = scan_frameStart(filePath, pid, unitStart_list, frameIndex_list);
    if(ret){
        printf("scan_frameStart error\n");
        return -1;
    }    
    
    std::vector<std::string> name_list;
    ret = cut_file(filePath, frameIndex_list, frameCount, destDir, name_list);   
    if(ret){
        printf("cut_file error\n");
        return -1;
    }
    
    ret = generate_m3u8(name_list, destDir, frameCount);
    if(ret){
        printf("generate_m3u8 error\n");
        return -1;
    }    
    
    return 0;
}


