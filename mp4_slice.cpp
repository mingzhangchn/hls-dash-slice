#include <stdio.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <stdlib.h>

#include "tinyxml2.h"
#include "mp4_slice.h"


typedef struct {
    std::string type;
    long pos;
    long size;
}BoxInfo;

typedef struct{
    unsigned int first_chunk;
    unsigned int samples_per_chunk;
    unsigned int sample_description_index;
}StscInfo;

typedef struct{
    unsigned int chunk_index;
    unsigned int offset;
    unsigned int sample_count;
    unsigned int sample_description_index;
}ChunkInfo;

typedef struct{
    unsigned int sample_index;
    unsigned int offset;
    unsigned int size;
    unsigned int delta;
    unsigned int chunk_index;
    unsigned int sample_description_index;
}SampleInfo;

typedef struct{
    unsigned int start;
    unsigned int size;
}SliceInfo;

static unsigned int byteToUint32(unsigned char buf[])
{
    return (unsigned int)buf[0] << 24 | (unsigned int)buf[1] << 16 | (unsigned int)buf[2]<<8 | buf[3];
}

static int set_box_size(int offset, int size, FILE *f){
    fseek(f, offset, SEEK_SET);
    unsigned char size_buf[4] = {0};
    size_buf[0] = (size & 0xff000000) >> 24;
    size_buf[1] = (size & 0x00ff0000) >> 16;
    size_buf[2] = (size & 0x0000ff00) >> 8;
    size_buf[3] = (size & 0x000000ff);
    fwrite(size_buf, 1, 4, f);           
}

static void saveBoxInfo(FILE *fp, long size, std::vector<BoxInfo> &info)
{
    int len = 0;
    while(size > 0 ? len < size: 1){
        unsigned char buf[16] = {0};
        if (fread(buf, 8, 1, fp) <= 0){
            printf("read finish\n");
            break;
        }

        BoxInfo tempInfo;
        std::string temp((const char*)&buf[4], 4);
        tempInfo.type = temp;
        tempInfo.pos = ftell (fp) - 8;
        tempInfo.size = byteToUint32(buf);
        info.push_back(tempInfo);
        
        printf("type:%s, size:%ld\n", tempInfo.type.c_str(), tempInfo.size);    
        if (fseek(fp, tempInfo.size - 8, SEEK_CUR) != 0){
            printf("seek error\n");
            break;
        }
        
        len += tempInfo.size;
    }    
}

int mp4_slice(const char *filePath, int frameCount, const char* destDir)
{
    if (!filePath || !destDir || frameCount < 0){
        printf("Input error\n");
        return -1;
    }
    
    FILE *fp = fopen(filePath, "r");
    std::vector<BoxInfo> rootBoxInfo;
    saveBoxInfo(fp, 0, rootBoxInfo);

    int media_duration_s = 0;
    int trackCount = 0;
    int vide_track = 0;
    int soun_track = 1;
    int v_timescale = 0;
    int s_timescale = 0;
    int vs_timescale[2] =  {0};
    
    std::vector<BoxInfo> moovBoxInfo;
    for(int i = 0; i < rootBoxInfo.size(); ++i){
        if (rootBoxInfo[i].type.compare("moov") == 0){
            if (fseek(fp, rootBoxInfo[i].pos + 8, SEEK_SET) != 0){
                printf("seek error\n");
                break;
            }
            saveBoxInfo(fp, rootBoxInfo[i].size - 8, moovBoxInfo);        
        }
    }
    
    for(int i = 0; i < moovBoxInfo.size(); ++i){
        if (moovBoxInfo[i].type.compare("trak") == 0){   
            trackCount++;        
        } 
    } 
    std::cout<<"trak count:"<<trackCount<<std::endl;
    
    int trackIndex = 0;
    std::vector<BoxInfo> trakBoxInfo[trackCount];
    for(int i = 0; i < moovBoxInfo.size(); ++i){
        if (moovBoxInfo[i].type.compare("trak") == 0){
            if (fseek(fp, moovBoxInfo[i].pos + 8, SEEK_SET) != 0){
                printf("seek error\n");
                break;
            }
            saveBoxInfo(fp, moovBoxInfo[i].size - 8, trakBoxInfo[trackIndex]);   
            trackIndex++;        
        } 
        
        if (moovBoxInfo[i].type.compare("mvhd") == 0){
            if (fseek(fp, moovBoxInfo[i].pos + 8, SEEK_SET) != 0){
                printf("seek error\n");
                break;
            }
            unsigned char tBuf[20] = {0};
            fread(tBuf, 1 , 20, fp);
            
            int scale = byteToUint32(&(tBuf[12]));
            int dural = byteToUint32(&(tBuf[16]));
            
            media_duration_s = dural/scale;
            printf("media_duration_s:%d\n", media_duration_s);
        }         
    }    

    std::vector<BoxInfo> mdiaBoxInfo[trackCount];
    for(int index = 0; index < trackCount; ++index){
        for(int i = 0; i < trakBoxInfo[index].size(); ++i){
            if (trakBoxInfo[index][i].type.compare("mdia") == 0){
                if (fseek(fp, trakBoxInfo[index][i].pos + 8, SEEK_SET) != 0){
                    printf("seek error\n");
                    break;
                }
                saveBoxInfo(fp, trakBoxInfo[index][i].size - 8, mdiaBoxInfo[index]);   
            }  
        }        
    }
    
    std::vector<BoxInfo> minfBoxInfo[trackCount];
    for(int index = 0; index < trackCount; ++index){
        for(int i = 0; i < mdiaBoxInfo[index].size(); ++i){
            if (mdiaBoxInfo[index][i].type.compare("minf") == 0){
                if (fseek(fp, mdiaBoxInfo[index][i].pos + 8, SEEK_SET) != 0){
                    printf("seek error\n");
                    break;
                }
                saveBoxInfo(fp, mdiaBoxInfo[index][i].size - 8, minfBoxInfo[index]);   
            }  
            if (mdiaBoxInfo[index][i].type.compare("hdlr") == 0){
                if (fseek(fp, mdiaBoxInfo[index][i].pos + 8, SEEK_SET) != 0){
                    printf("seek error\n");
                    break;
                }
                fseek(fp, 8, SEEK_CUR);
                char buf[4] = {0};
                fread(buf, 4, 1, fp);
                printf("track[%d]:%s\n", index, buf);
                if (strncmp(buf, "vide", 4) == 0){
                    vide_track = index;
                }
                if (strncmp(buf, "soun", 4) == 0){
                    soun_track = index;
                }                
            }  
        }        
    }
    
    for(int index = 0; index < trackCount; ++index){
        for(int i = 0; i < mdiaBoxInfo[index].size(); ++i){  
            if (mdiaBoxInfo[index][i].type.compare("mdhd") == 0){
                if (fseek(fp, mdiaBoxInfo[index][i].pos + 8, SEEK_SET) != 0){
                    printf("seek error\n");
                    break;
                }
                fseek(fp, 12, SEEK_CUR);
                unsigned char buf[4] = {0};
                fread(buf, 4, 1, fp);
                if (index == vide_track){ //can only be used when vide and soun be set properly
                    v_timescale = byteToUint32(buf);
                    printf("v_timescale:%d\n", v_timescale);
                    vs_timescale[0] = v_timescale;
                }
                if (index == soun_track){
                    s_timescale = byteToUint32(buf);
                    printf("s_timescale:%d\n", s_timescale);
                    vs_timescale[1] = s_timescale;
                }                
            }  
        }        
    }    
    
    std::vector<BoxInfo> stblBoxInfo[trackCount];
    for(int index = 0; index < trackCount; ++index){
        for(int i = 0; i < minfBoxInfo[index].size(); ++i){
            if (minfBoxInfo[index][i].type.compare("stbl") == 0){
                if (fseek(fp, minfBoxInfo[index][i].pos + 8, SEEK_SET) != 0){
                    printf("seek error\n");
                    break;
                }
                saveBoxInfo(fp, minfBoxInfo[index][i].size - 8, stblBoxInfo[index]);   
            }  
        }        
    }    
    
    std::vector<StscInfo> stscList[trackCount];
    std::vector<unsigned int> stcoList[trackCount];
    std::vector<unsigned int> stszList[trackCount];
    unsigned int chunkTotalCount[trackCount];
    unsigned int sampleTotalCount[trackCount];
    ChunkInfo *chunkList[trackCount];
    SampleInfo *sampleList[trackCount];
    for(int i = 0; i < trackCount; ++i){//init
        chunkList[i] = NULL;
        sampleList[i] = NULL;
    }    
    
    std::vector<SliceInfo> slice_list;
    
    //int tIndex = 0;
    for(int tIndex = 0; tIndex < trackCount; ++tIndex)
    {
        int sampleCount = 0;
        int chunkCount = 0;
        printf("\n-------Track %d------\n", tIndex);
        for(int i = 0; i < stblBoxInfo[tIndex].size(); ++i){
            if (stblBoxInfo[tIndex][i].type.compare("stsc") == 0){
                if (fseek(fp, stblBoxInfo[tIndex][i].pos + 8, SEEK_SET) != 0){
                    printf("seek error\n");
                    break;
                }
                fseek(fp, 4, SEEK_CUR);
                unsigned char buf[4] = {0};
                int ret = fread(buf, 4, 1, fp);

                int entry_count = byteToUint32(buf);
                std::cout<<"stsc entry count:"<<entry_count<<std::endl;
                int count = 0;
                while(count < entry_count){
                    unsigned char tempBuf[12] = {0};
                    fread(tempBuf, 12, 1, fp);
                    StscInfo info;
                    info.first_chunk = byteToUint32(tempBuf);
                    info.samples_per_chunk = byteToUint32(tempBuf + 4);
                    info.sample_description_index = byteToUint32(tempBuf + 8);
                    stscList[tIndex].push_back(info);
                    //printf("%d:%d:%d\n", info.first_chunk, info.samples_per_chunk, info.sample_description_index);
                    count++;
                }
            }  
         
            if (stblBoxInfo[tIndex][i].type.compare("stco") == 0){
                if (fseek(fp, stblBoxInfo[tIndex][i].pos + 8, SEEK_SET) != 0){
                    printf("seek error\n");
                    break;
                }
                fseek(fp, 4, SEEK_CUR);
                unsigned char buf[4] = {0};
                int ret = fread(buf, 4, 1, fp);

                int entry_count = byteToUint32(buf);
                std::cout<<"stco entry count:"<<entry_count<<std::endl;
                int count = 0;
                while(count < entry_count){
                    unsigned char tempBuf[4] = {0};
                    fread(tempBuf, 4, 1, fp);

                    unsigned int offset = byteToUint32(tempBuf);

                    stcoList[tIndex].push_back(offset);
                    //printf("Index : %d, offset:%d\n", count, offset);
                    count++;
                }
                chunkTotalCount[tIndex] = stcoList[tIndex].size();
                chunkCount = chunkTotalCount[tIndex];
                chunkList[tIndex] = (ChunkInfo*)malloc(sizeof(ChunkInfo)*chunkCount);
            }  
            
            if (stblBoxInfo[tIndex][i].type.compare("stsz") == 0){
                if (fseek(fp, stblBoxInfo[tIndex][i].pos + 8, SEEK_SET) != 0){
                    printf("seek error\n");
                    break;
                }
                fseek(fp, 4, SEEK_CUR);
                unsigned char buf[8] = {0};
                int ret = fread(buf, 8, 1, fp);
                int sample_size = byteToUint32(buf);
                int entry_count = byteToUint32(buf + 4);
                std::cout<<"stsz sample size:"<<sample_size<< ", entry count:"<<entry_count<<std::endl;
                int count = 0;
                while(count < entry_count){
                    unsigned char tempBuf[4] = {0};
                    fread(tempBuf, 4, 1, fp);

                    unsigned int entry_size = byteToUint32(tempBuf);
                    stszList[tIndex].push_back(entry_size);
                    
                    //printf("Index: %d, entry_size:%d\n", count, entry_size);
                    count++;
                }
                sampleTotalCount[tIndex] = stszList[tIndex].size();
                sampleCount = sampleTotalCount[tIndex];
                sampleList[tIndex] = (SampleInfo*)malloc(sizeof(SampleInfo)*sampleCount);
            }  
        }         

        if (!sampleList[tIndex] || !chunkList[tIndex]){
            printf("sampleList or chunkList malloc error\n");
            return -1;
        }
        
        for(int i = 0; i < stblBoxInfo[tIndex].size(); ++i){//make sure  sampleList has been created
            if (stblBoxInfo[tIndex][i].type.compare("stts") == 0){
                if (fseek(fp, stblBoxInfo[tIndex][i].pos + 8, SEEK_SET) != 0){
                    printf("seek error\n");
                    break;
                }
                fseek(fp, 4, SEEK_CUR);
                unsigned char buf[4] = {0};
                int ret = fread(buf, 4, 1, fp);
                int entry_count = byteToUint32(buf);
                std::cout<<"stts entry count:"<<entry_count<<std::endl;
                
                if (entry_count == 1){
                    unsigned char tempBuf[8] = {0};
                    fread(tempBuf, 1, 8, fp);      
                    unsigned int sample_count = byteToUint32(tempBuf);
                    unsigned int sample_delta = byteToUint32(tempBuf + 4);
                    //printf("1 sample_count: %d, sample_delta:%d\n", sample_count, sample_delta); 
                    int sample_index = 0;
                    while(sample_index < sampleTotalCount[tIndex]){
                        sampleList[tIndex][sample_index].delta = sample_delta;
                        sample_index++;
                    }
                }else if(entry_count > 1){
                    int sample_index = 0;
                    int entry_count_index = 0;
                    while(entry_count_index < entry_count){
                        unsigned char tempBuf[8] = {0};
                        fread(tempBuf, 1, 8, fp);
                        unsigned int sample_count = byteToUint32(tempBuf);
                        unsigned int sample_delta = byteToUint32(tempBuf + 4);
                        //printf("2 sample_count: %d, sample_delta:%d\n", sample_count, sample_delta);  
                        
                        int sample_count_index = 0;
                        while(sample_count_index < sample_count && sample_index < sampleTotalCount[tIndex]){
                            sampleList[tIndex][sample_index].delta = sample_delta;
                            sample_index++;
                            sample_count_index++;
                        }                        
                        
                        entry_count_index++;
                    }                    
                }
                else{
                    printf("No stts entry found\n");
                    return -1;
                }
            }              
        }        
        
        printf("chunk:%d, sample:%d\n", chunkCount, sampleCount);

        ChunkInfo *chunk_list = chunkList[tIndex];
        SampleInfo *sample_list = sampleList[tIndex];
        
        if (stscList[tIndex].size() == 1){
            if (chunkCount == sampleCount){
                for (int i = 0; i < chunkCount; ++i){
                    chunk_list[i].chunk_index = i + 1;
                    chunk_list[i].sample_count = 1;
                    chunk_list[i].sample_description_index = 1;
                    
                    sample_list[i].sample_index = i + 1;
                    sample_list[i].chunk_index = i + 1;
                    sample_list[i].sample_description_index = 1;
                }      
            }
            else{
                printf("Not process yet!\n");
            }
        }
        else{
            int chunkIndex = 0; 
            int lastChunkIndex = chunkCount + 1;
            for (int i = stscList[tIndex].size() - 1; i >= 0 ; i--){
                int count = lastChunkIndex - stscList[tIndex][i].first_chunk;
                
                chunkIndex = lastChunkIndex - 1 - 1;
                while(count > 0 ){
                    chunk_list[chunkIndex].chunk_index = chunkIndex + 1;
                    chunk_list[chunkIndex].sample_count = stscList[tIndex][i].samples_per_chunk;
                    chunk_list[chunkIndex].sample_description_index = stscList[tIndex][i].sample_description_index;
                    count--;
                    chunkIndex--;
                }
                
                lastChunkIndex = stscList[tIndex][i].first_chunk;
            }
            
            int sampleIndex = 0;
            for (int i = 0; i < chunkCount; ++i){
                int count = chunk_list[i].sample_count;
                while(count > 0){
                    sample_list[sampleIndex].sample_index = sampleIndex + 1;
                    sample_list[sampleIndex].chunk_index = chunk_list[i].chunk_index;
                    sample_list[sampleIndex].sample_description_index = chunk_list[i].sample_description_index;
                    count--;
                    sampleIndex++;
                }
            }
        }
        
        for (int i = 0; i < chunkCount; ++i){
            chunk_list[i].offset = stcoList[tIndex][i];
        }
        
        for (int i = 0; i < sampleCount; ++i){
            sample_list[i].size = stszList[tIndex][i];//this size will be used latter
            
            int chunkIndex = sample_list[i].chunk_index;
            int chunkOffset = chunk_list[chunkIndex -1].offset;
            int size = 0;
            for(int j = i -1; j >= 0; j--){
                if (sample_list[j].chunk_index == chunkIndex){
                    size += sample_list[j].size;//use size
                }else{
                    break;
                }
            }
            
            sample_list[i].offset = chunkOffset + size;
            //printf("\nsample:%d,offset:%d, size:%d\n", i+1, sample_list[i].offset, sample_list[i].size);
            
            if(0){//test
                if (fseek(fp, sample_list[i].offset, SEEK_SET) != 0){
                    printf("seek error\n");
                }    
                
                unsigned char buf[16] = {0};
                int ret = fread(buf, 1, 16, fp);   
                if (ret != 16){
                    printf("read error\n");
                }
                printf("\n");
                for (int k = 0; k < 16; ++k){
                    printf("%02x ", buf[k]);
                }
            }
        }
        
        std::vector<int> sliceIndex;
        if (tIndex == vide_track && sampleCount > 0){
            int count = 0;
            for(int i = 0; i < sampleCount; ++i){
                if (i%frameCount == 0){
                    //printf("frame index %d \n", i);
                    sliceIndex.push_back(i);
                }
            }
            
            if ((sampleCount - 1)%frameCount != 0){
                sliceIndex.push_back(sampleCount - 1);
            }
            
            SliceInfo sInfo;
            sInfo.start =  sample_list[0].offset;
            for(int i = 1; i < sliceIndex.size(); ++i){
                sInfo.size = sample_list[sliceIndex[i]].offset - sInfo.start;
                //printf("start:%d, size:%d\n", sInfo.start, sInfo.size);
                slice_list.push_back(sInfo);
                sInfo.start = sample_list[sliceIndex[i]].offset;
            }            
        }
    }

    //generate init segment
    {
        printf("Generate init segment\n");
        char slice_file[1024] = {0};
        snprintf(slice_file, sizeof(slice_file), "%s/init.mp4", destDir);
        
        FILE *fp2 = fopen(slice_file, "w+");
        if(!fp2){
            printf("open file error\n");
            return -1;
        }
        
        unsigned char ftypBuf[28] = {0};
        ftypBuf[0] = 0; ftypBuf[1] = 0; ftypBuf[2] = 0; ftypBuf[3] = 0x1c;
        ftypBuf[4] = 'f'; ftypBuf[5] = 't'; ftypBuf[6] = 'y'; ftypBuf[7] = 'p';
        ftypBuf[8] = 'i'; ftypBuf[9] = 's'; ftypBuf[10] = 'o'; ftypBuf[11] = '5';
        ftypBuf[12] = 0; ftypBuf[13] = 0; ftypBuf[14] = 0; ftypBuf[15] = 0x01;
        ftypBuf[16] = 'a'; ftypBuf[17] = 'v'; ftypBuf[18] = 'c'; ftypBuf[19] = '1';
        ftypBuf[20] = 'i'; ftypBuf[21] = 's'; ftypBuf[22] = 'o'; ftypBuf[23] = '5';
        ftypBuf[24] = 'd'; ftypBuf[25] = 'a'; ftypBuf[26] = 's'; ftypBuf[27] = 'h';
        
        fwrite(ftypBuf, 1, 28, fp2);
        
        int first_v_sample_offset = 0;
        int fileEndOffset = 0;
        
        int mdat_start = 0;
        int moov_start = 0;
        int mvhd_start = 0;
        int mvex_start = 0;
        
        unsigned int duration = 0 ;
        
        moov_start = ftell(fp2);
        fwrite("0000moov", 1, 8, fp2);//size set latter
        
        mvhd_start = ftell(fp2);
        unsigned char *mvhd_buf = NULL;
        int mvhd_size = 0;
        int tDuration = 0;
        for(int i = 0; i < moovBoxInfo.size(); ++i){
            if (moovBoxInfo[i].type.compare("mvhd") == 0){   
                fseek(fp, moovBoxInfo[i].pos, SEEK_SET);
                mvhd_size = moovBoxInfo[i].size;
                printf("mvhd_size:%d\n", mvhd_size);
                mvhd_buf = (unsigned char*)malloc(mvhd_size);
                fread(mvhd_buf, 1, mvhd_size, fp);
                break;
            } 
        }
        if (mvhd_buf){
            mvhd_buf[20] = 0x00;
            mvhd_buf[21] = 0x00;
            mvhd_buf[22] = 0x03;
            mvhd_buf[23] = 0xe8;
            
            tDuration = byteToUint32(&(mvhd_buf[24]));
            mvhd_buf[24] = (duration & 0xff000000) >> 24;
            mvhd_buf[25] = (duration & 0x00ff0000) >> 16;;
            mvhd_buf[26] = (duration & 0x0000ff00) >> 8;;
            mvhd_buf[27] = (duration & 0x000000ff);      
            
            fwrite(mvhd_buf, 1, mvhd_size, fp2);
            free(mvhd_buf);
        }
        else{
            printf("mvhd_buf error happens\n");
            return -1;
        }
        
        mvex_start = ftell(fp2);
        fwrite("0000mvex", 1, 8, fp2);//size set latter
        int dur = tDuration;
        unsigned char mehdBuf[16] = {0};
        mehdBuf[0] = 0; mehdBuf[1] = 0; mehdBuf[2] = 0x00; mehdBuf[3] = 0x10;
        mehdBuf[4] = 'm';mehdBuf[5] = 'e';mehdBuf[6] = 'h';mehdBuf[7] = 'd';
        mehdBuf[12] = (dur & 0xff000000) >> 24;
        mehdBuf[13] = (dur & 0x00ff0000) >> 16;
        mehdBuf[14] = (dur & 0x0000ff00) >> 8;
        mehdBuf[15] = (dur & 0x000000ff);
        fwrite(mehdBuf, 1, 16, fp2);
        
        unsigned char trexBuf[32] = {0};
        trexBuf[0] = 0; trexBuf[1] = 0; trexBuf[2] = 0x00; trexBuf[3] = 0x20;
        trexBuf[4] = 't';trexBuf[5] = 'r';trexBuf[6] = 'e';trexBuf[7] = 'x';    
        
        //
        trexBuf[12] = 0; trexBuf[13] = 0; trexBuf[14] = 0x00; trexBuf[15] = 0x01; 
        trexBuf[16] = 0; trexBuf[17] = 0; trexBuf[17] = 0x00; trexBuf[19] = 0x01;
        //v delta
        trexBuf[20] = 0; 
        trexBuf[21] = 0; 
        trexBuf[22] = 0x0e; 
        trexBuf[23] = 0xa6;
        trexBuf[24] = 0; trexBuf[25] = 0; trexBuf[26] = 0x00; trexBuf[27] = 0x00;
        trexBuf[28] = 0; trexBuf[29] = 0x01; trexBuf[30] = 0x00; trexBuf[31] = 0x00;  
        fwrite(trexBuf, 1, 32, fp2);
        
        trexBuf[12] = 0; trexBuf[13] = 0; trexBuf[14] = 0x00; trexBuf[15] = 0x02; 
        trexBuf[16] = 0; trexBuf[17] = 0; trexBuf[17] = 0x00; trexBuf[19] = 0x01;
        // s delta
        trexBuf[20] = 0; 
        trexBuf[21] = 0; 
        trexBuf[22] = 0x04; 
        trexBuf[23] = 0x00;
        trexBuf[24] = 0; trexBuf[25] = 0; trexBuf[26] = 0x00; trexBuf[27] = 0x00;
        trexBuf[28] = 0x02; trexBuf[29] = 0; trexBuf[30] = 0x00; trexBuf[31] = 0x00;         
        fwrite(trexBuf, 1, 32, fp2);
        
        unsigned char trepBuf[16] = {0};
        mehdBuf[0] = 0; mehdBuf[1] = 0; mehdBuf[2] = 0x00; mehdBuf[3] = 0x10;
        mehdBuf[4] = 't';mehdBuf[5] = 'r';mehdBuf[6] = 'e';mehdBuf[7] = 'p';        
        mehdBuf[8] = 0; mehdBuf[9] = 0; mehdBuf[10] = 0; mehdBuf[11] = 0;
        mehdBuf[12] = 0;mehdBuf[13] = 0;mehdBuf[14] = 0;mehdBuf[15] = 0x01;    
        fwrite(mehdBuf, 1, 16, fp2);        
        mehdBuf[12] = 0;mehdBuf[13] = 0;mehdBuf[14] = 0;mehdBuf[15] = 0x02;    
        fwrite(mehdBuf, 1, 16, fp2);     

        fileEndOffset = ftell(fp2);
        set_box_size(mvex_start, (fileEndOffset - mvex_start), fp2);   
        
        for(int track_index = 0; track_index < trackCount; ++track_index){
            int trak_start_offset = 0;
            int mdia_start_offset = 0;
            int minf_start_offset = 0;
            int stbl_start_offset = 0;  
            
            fseek(fp2, fileEndOffset, SEEK_SET);
            
            printf("generate trak!\n");
            trak_start_offset = ftell(fp2);
            fwrite("0000trak", 1, 8, fp2);//size set latter
            
            printf("generate tkhd!\n");
            for(int i = 0; i < trakBoxInfo[track_index].size(); ++i){
                if (trakBoxInfo[track_index][i].type.compare("tkhd") == 0){  
                    fseek(fp, trakBoxInfo[track_index][i].pos, SEEK_SET);
                    int count = 0;
                    while(count < trakBoxInfo[track_index][i].size){
                        fputc(fgetc(fp), fp2);
                        count++;
                    }
                    break;
                }
            }
            
            printf("generate mdia!\n");
            mdia_start_offset = ftell(fp2);
            fwrite("0000mdia", 1, 8, fp2);//size set latter        
            
            unsigned char mdhd_buf[32] = {0};
            mdhd_buf[0] = 0; mdhd_buf[1] = 0; mdhd_buf[2] = 0; mdhd_buf[3] = 0x20;
            mdhd_buf[4] = 'm'; mdhd_buf[5] = 'd'; mdhd_buf[6] = 'h'; mdhd_buf[7] = 'd';
            mdhd_buf[20] = (vs_timescale[track_index] & 0xff000000) >> 24; 
            mdhd_buf[21] = (vs_timescale[track_index] & 0x00ff0000) >> 16;
            mdhd_buf[22] = (vs_timescale[track_index] & 0x0000ff00) >> 8;
            mdhd_buf[23] = vs_timescale[track_index] & 0x000000ff;
            int v_du = 0;
            mdhd_buf[24] = (v_du & 0xff000000) >> 24; 
            mdhd_buf[25] = (v_du & 0x00ff0000) >> 16;
            mdhd_buf[26] = (v_du & 0x0000ff00) >> 8;
            mdhd_buf[27] = v_du & 0x000000ff;        
            fwrite(mdhd_buf, 1, 32, fp2);
            
            for(int i = 0; i < mdiaBoxInfo[track_index].size(); ++i){
                if (mdiaBoxInfo[track_index][i].type.compare("hdlr") == 0){  
                    int count = 0;
                    fseek(fp, mdiaBoxInfo[track_index][i].pos, SEEK_SET);
                    while(count < mdiaBoxInfo[track_index][i].size){
                        fputc(fgetc(fp), fp2);
                        count++;
                    }
                    break;
                }
            }        
            
            printf("generate minf!\n");
            minf_start_offset = ftell(fp2);
            fwrite("0000minf", 1, 8, fp2);//size set latter
            for(int i = 0; i < minfBoxInfo[track_index].size(); ++i){
                if (minfBoxInfo[track_index][i].type.compare("vmhd") == 0){
                    int count = 0;
                    fseek(fp, minfBoxInfo[track_index][i].pos, SEEK_SET);
                    while(count < minfBoxInfo[track_index][i].size){
                        fputc(fgetc(fp), fp2);
                        count++;
                    }
                    break;  
                }  
            }
            
            for(int i = 0; i < minfBoxInfo[track_index].size(); ++i){
                if (minfBoxInfo[track_index][i].type.compare("smhd") == 0){
                    int count = 0;
                    fseek(fp, minfBoxInfo[track_index][i].pos, SEEK_SET);
                    while(count < minfBoxInfo[track_index][i].size){
                        fputc(fgetc(fp), fp2);
                        count++;
                    }
                    break;  
                }  
            }            

            for(int i = 0; i < minfBoxInfo[track_index].size(); ++i){
                if (minfBoxInfo[track_index][i].type.compare("dinf") == 0){
                    int count = 0;
                    fseek(fp, minfBoxInfo[track_index][i].pos, SEEK_SET);
                    while(count < minfBoxInfo[track_index][i].size){
                        fputc(fgetc(fp), fp2);
                        count++;
                    }
                    break;  
                }  
            }        
            
            printf("generate stbl!\n");
            stbl_start_offset = ftell(fp2);
            fwrite("0000stbl", 1, 8, fp2);//size set latter
            
            for(int i = 0; i < stblBoxInfo[track_index].size(); ++i){
                if (stblBoxInfo[track_index][i].type.compare("stsd") == 0){
                    int count = 0;
                    fseek(fp, stblBoxInfo[track_index][i].pos, SEEK_SET);
                    while(count < stblBoxInfo[track_index][i].size){
                        fputc(fgetc(fp), fp2);
                        count++;
                    }
                    break;                 
                }
            }
            
            unsigned char stts_buf[16] = {0};
            stts_buf[0] = 0; stts_buf[1] = 0; stts_buf[2] = 0; stts_buf[3] = 0x10;
            stts_buf[4] = 's'; stts_buf[5] = 't'; stts_buf[6] = 't'; stts_buf[7] = 's';   
            fwrite(stts_buf, 1, 16, fp2);

            unsigned char stsc_buf[16] = {0};
            stsc_buf[0] = 0; stsc_buf[1] = 0; stsc_buf[2] = 0; stsc_buf[3] = 0x10;
            stsc_buf[4] = 's'; stsc_buf[5] = 't'; stsc_buf[6] = 's'; stsc_buf[7] = 'c';
            stsc_buf[12] = 0; stsc_buf[13] = 0; stsc_buf[14] = 0; stsc_buf[15] = 0x00;
            
            fwrite(stsc_buf, 1, 16, fp2);
            
            int stsz_sample_count = 0;
            int stsz_size = 20  + stsz_sample_count * 4;
            unsigned char stsz_buf[20] = {0};
            stsz_buf[0] = (stsz_size & 0xff000000) >> 24; 
            stsz_buf[1] = (stsz_size & 0x00ff0000) >> 16; 
            stsz_buf[2] = (stsz_size & 0x0000ff00) >> 8; 
            stsz_buf[3] = (stsz_size & 0x000000ff);
            stsz_buf[4] = 's'; stsz_buf[5] = 't'; stsz_buf[6] = 's'; stsz_buf[7] = 'z';
            stsz_buf[12] = 0; stsz_buf[13] = 0; stsz_buf[14] = 0; stsz_buf[15] = 0;
            stsz_buf[16] = (stsz_sample_count & 0xff000000) >> 24;  
            stsz_buf[17] = (stsz_sample_count & 0x00ff0000) >> 16; 
            stsz_buf[18] = (stsz_sample_count & 0x0000ff00) >> 8;  
            stsz_buf[19] = (stsz_sample_count & 0x000000ff);  
            fwrite(stsz_buf, 1, 20, fp2);
            
            int stco_entry_count = 0;
            int stco_size = 16  + stco_entry_count * 4;
            printf("stco offset:%ld, stco_size:0x%x\n", ftell(fp2),  stco_size);
            unsigned char stco_buf[16] = {0};
            stco_buf[0] = (stco_size & 0xff000000) >> 24; 
            stco_buf[1] = (stco_size & 0x00ff0000) >> 16; 
            stco_buf[2] = (stco_size & 0x0000ff00) >> 8; 
            stco_buf[3] = (stco_size & 0x000000ff);
            stco_buf[4] = 's'; stco_buf[5] = 't'; stco_buf[6] = 'c'; stco_buf[7] = 'o';
            stco_buf[12] = (stco_entry_count & 0xff000000) >> 24;
            stco_buf[13] = (stco_entry_count & 0x00ff0000) >> 16;
            stco_buf[14] = (stco_entry_count & 0x0000ff00) >> 8;
            stco_buf[15] = (stco_entry_count & 0x000000ff);
            fwrite(stco_buf, 1, 16, fp2);
            
            fileEndOffset = ftell(fp2);
            
            set_box_size(stbl_start_offset, (fileEndOffset - stbl_start_offset), fp2);
            printf("stbs offset:%x, size:%d\n", stbl_start_offset, fileEndOffset - stbl_start_offset);
            
            set_box_size(minf_start_offset, (fileEndOffset - minf_start_offset), fp2); 
            printf("minf offset:%x, size:%d\n", minf_start_offset, fileEndOffset - minf_start_offset);            
            
            set_box_size(mdia_start_offset, (fileEndOffset - mdia_start_offset), fp2);
            printf("mdia offset:%x, size:%d\n", mdia_start_offset, fileEndOffset - mdia_start_offset);
       
            set_box_size(trak_start_offset, (fileEndOffset - trak_start_offset), fp2);   
            printf("trak offset:%x, size:%d\n", trak_start_offset, fileEndOffset - trak_start_offset);
        }
        
        set_box_size(moov_start, (fileEndOffset - moov_start), fp2);
        printf("moov offset:%x, size:%d\n", moov_start, fileEndOffset - moov_start);
        
        fclose(fp2);
    }
      
    //generate media segment
    int slice_duration = 0;
    for(int sliceIndex = 0; sliceIndex < slice_list.size(); ++sliceIndex)
    //int sliceIndex = 0;
    {
        int fileEndOffset = 0;
        int sidex_start = 0;
        int sidex_reference_start = 0;
        int moof_start = 0;
        int moof_size = 0;
        int mdat_start = 0;
        int mdat_size = 0;
        int subsegment_duration = 0;
                
        char slice_file[1024] = {0};
        snprintf(slice_file, sizeof(slice_file), "%s/%08d.m4s", destDir, sliceIndex+1);
        
        printf("creat slice %d\n", sliceIndex);
        FILE *fp2 = fopen(slice_file, "w+");
        if(!fp2){
            printf("open file error\n");
            return -1;
        }
        
        unsigned char stypBuf[24] = {0};
        stypBuf[0] = 0; stypBuf[1] = 0; stypBuf[2] = 0; stypBuf[3] = 0x18;
        stypBuf[4] ='s';stypBuf[5] ='t';stypBuf[6] ='y';stypBuf[7] = 'p';
        stypBuf[8] ='m';stypBuf[9] ='s';stypBuf[10] ='d';stypBuf[11] = 'h';
        stypBuf[16] ='m';stypBuf[17] ='s';stypBuf[18] ='d';stypBuf[19] = 'h';
        stypBuf[20] ='m';stypBuf[21] ='s';stypBuf[22] ='i';stypBuf[23] = 'x';
        fwrite(stypBuf, 1, 24, fp2);
        
        sidex_start = ftell(fp2);
        int reference_count = 1;
        int sidexSize = 32 + reference_count * 12;
        unsigned char sidxBuf[32] = {0};
        sidxBuf[0] = (sidexSize & 0xff000000) >> 24; 
        sidxBuf[1] = (sidexSize & 0x00ff0000) >> 16;  
        sidxBuf[2] = (sidexSize & 0x0000ff00) >> 8;  
        sidxBuf[3] = (sidexSize & 0x000000ff) ; 
        sidxBuf[4] ='s';sidxBuf[5] ='i';sidxBuf[6] ='d';sidxBuf[7] = 'x';
        sidxBuf[12] =0; sidxBuf[13] =0; sidxBuf[14] =0; sidxBuf[15] = 0x01;
        //timescale
        sidxBuf[16] =0;sidxBuf[17] =0;sidxBuf[18] =0x03;sidxBuf[19] = 0xe8;
        //earliest_presentation_time set latter
        sidxBuf[20] = 0;
        sidxBuf[21] = 0;
        sidxBuf[22] = 0;
        sidxBuf[23] = 0;        
        sidxBuf[28] =0;sidxBuf[29] =0;
        sidxBuf[30] = (reference_count & 0xff00) >> 4;
        sidxBuf[31] = reference_count & 0x00ff;
        fwrite(sidxBuf, 1, 32, fp2);
        
        sidex_reference_start = ftell(fp2);
        for(int i = 0; i<reference_count; ++i){
            unsigned char buf[12] = {0};
            fwrite(buf, 1, 12, fp2);
        }
        
        moof_start = ftell(fp2);
        unsigned char moofBuf[8] = {0};
        moofBuf[0] = 0; moofBuf[1] = 0; moofBuf[2] = 0; moofBuf[3] = 0;//size set latter
        moofBuf[4] ='m';moofBuf[5] ='o';moofBuf[6] ='o';moofBuf[7] = 'f';
        fwrite(moofBuf, 1, 8, fp2);
        
        unsigned char mfhdBuf[16] = {0};
        mfhdBuf[0] = 0; mfhdBuf[1] = 0; mfhdBuf[2] = 0; mfhdBuf[3] = 0x10;
        mfhdBuf[4] ='m';mfhdBuf[5] ='f';mfhdBuf[6] ='h';mfhdBuf[7] = 'd';
        int seq_number = sliceIndex  + 1;
        mfhdBuf[12] =(seq_number & 0xff000000) >> 24; 
        mfhdBuf[13] =(seq_number & 0x00ff0000) >> 16;
        mfhdBuf[14] =(seq_number & 0x0000ff00) >> 8;
        mfhdBuf[15] =(seq_number & 0x000000ff) ;        
        fwrite(mfhdBuf, 1, 16, fp2);
        
        int v_trun_offset = 0;
        int s_trun_offset = 0;
        
        for(int track = 0; track < 2; ++track){
            int traf_start = ftell(fp2);
            unsigned char trafBuf[8] = {0};
            trafBuf[0] = 0; trafBuf[1] = 0; trafBuf[2] = 0; trafBuf[3] = 0;//size set latter
            trafBuf[4] ='t';trafBuf[5] ='r';trafBuf[6] ='a';trafBuf[7] = 'f';
            fwrite(trafBuf, 1, 8, fp2);        

            unsigned char tfhdBuf[16] = {0};
            tfhdBuf[0] = 0; tfhdBuf[1] = 0; tfhdBuf[2] = 0; tfhdBuf[3] = 0x10;
            tfhdBuf[4] ='t';tfhdBuf[5] ='f';tfhdBuf[6] ='h';tfhdBuf[7] = 'd';
            tfhdBuf[8] = 0; tfhdBuf[9] = 0x02; tfhdBuf[10] = 0; tfhdBuf[11] = 0;
            tfhdBuf[12] = 0; tfhdBuf[13] = 0; tfhdBuf[14] = 0; tfhdBuf[15] = (track + 1);
            fwrite(tfhdBuf, 1, 16, fp2);              

            std::vector<int> indexList;
            for(int i = 0; i < sampleTotalCount[track]; ++i){
                if (sampleList[track][i].offset >= slice_list[sliceIndex].start 
                   &&sampleList[track][i].offset < (slice_list[sliceIndex].start + slice_list[sliceIndex].size)){
                       indexList.push_back(i);
                }
            } 
            
            //canculate time base
            int time_base = 0;
            int last_sample_index = indexList[0];
            
            for(int i = 0; i < last_sample_index; ++i){
                time_base += sampleList[track][i].delta;
            }
            //printf("sample count:%d, time base:%d\n", indexList.size(), time_base);
            
            unsigned char tfdtBuf[16] = {0};
            tfdtBuf[0] = 0; tfdtBuf[1] = 0; tfdtBuf[2] = 0; tfdtBuf[3] = 0x10;
            tfdtBuf[4] ='t';tfdtBuf[5] ='f';tfdtBuf[6] ='d';tfdtBuf[7] = 't';
            tfdtBuf[12] = (time_base & 0xff000000) >>24;
            tfdtBuf[13] = (time_base & 0x00ff0000) >>16;
            tfdtBuf[14] = (time_base & 0x0000ff00) >>8;
            tfdtBuf[15] = (time_base & 0x000000ff) ;
            fwrite(tfdtBuf, 1, 16, fp2);              
            
            if (track == 0){
                v_trun_offset = ftell(fp2);
            }
            else{
                s_trun_offset = ftell(fp2);
            }
            
            if (track == vide_track){
                //set earliest_presentation_time
                fileEndOffset = ftell(fp2);
                
                fseek(fp2, sidex_start + 20, SEEK_SET);
                unsigned char buf[4] = {0};
                buf[0] = (time_base & 0xff000000) >>24;
                buf[1] = (time_base & 0x00ff0000) >>16;
                buf[2] = (time_base & 0x0000ff00) >>8;
                buf[3] = (time_base & 0x000000ff) ;                
                fwrite(buf, 1, 4, fp2);
        
                fseek(fp2, fileEndOffset, SEEK_SET);
                
                //subsegment_duration canculate
                for(int i = 0; i < indexList.size(); ++i){
                    subsegment_duration += sampleList[track][indexList[i]].delta;
                }  
                slice_duration = subsegment_duration;           
            }
            
            unsigned char trunBuf[20] = {0};
            int trun_sample_count = indexList.size();
            int trunSize = 20 + trun_sample_count * 8;
            trunBuf[0] = (trunSize & 0xff000000) >> 24; 
            trunBuf[1] = (trunSize & 0x00ff0000) >> 16;  
            trunBuf[2] = (trunSize & 0x0000ff00) >> 8;  
            trunBuf[3] = (trunSize & 0x000000ff) ; 
            trunBuf[4] ='t';trunBuf[5] ='r';trunBuf[6] ='u';trunBuf[7] = 'n';
            trunBuf[8] = 0; trunBuf[9] = 0; trunBuf[10] = 0x03; trunBuf[11] = 0x01;  
            trunBuf[12] = (trun_sample_count & 0xff000000) >> 24;
            trunBuf[13] = (trun_sample_count & 0x00ff0000) >> 16;
            trunBuf[14] = (trun_sample_count & 0x0000ff00) >> 8;
            trunBuf[15] = (trun_sample_count & 0x00000ff);
            fwrite(trunBuf, 1, 20, fp2);    
            for (int i = 0; i < trun_sample_count; ++i){
                int sample_duration = sampleList[track][indexList[i]].delta;
                int sample_size = sampleList[track][indexList[i]].size;
                unsigned char tBuf[8] = {0};
                tBuf[0] = (sample_duration & 0xff000000) >> 24;
                tBuf[1] = (sample_duration & 0x00ff0000) >> 16;
                tBuf[2] = (sample_duration & 0x0000ff00) >> 8;
                tBuf[3] = (sample_duration & 0x000000ff) ;
                tBuf[4] = (sample_size & 0xff000000) >> 24;
                tBuf[5] = (sample_size & 0x00ff0000) >> 16;
                tBuf[6] = (sample_size & 0x0000ff00) >> 8;
                tBuf[7] = (sample_size & 0x000000ff) ;                
                fwrite(tBuf, 1, 8, fp2);
            }
            fileEndOffset = ftell(fp2);
            
            set_box_size(traf_start, (fileEndOffset - traf_start), fp2);
            fseek(fp2, fileEndOffset, SEEK_SET);
        }
       
        fileEndOffset = ftell(fp2);
        moof_size = fileEndOffset - moof_start;
        set_box_size(moof_start, (fileEndOffset - moof_start), fp2);  
        fseek(fp2, fileEndOffset, SEEK_SET);
        
        mdat_start = ftell(fp2);
        unsigned char mdatBuf[8] = {0};
        mdatBuf[0] = 0; mdatBuf[1] = 0; mdatBuf[2] = 0; mdatBuf[3] = 0;//size set latter
        mdatBuf[4] ='m';mdatBuf[5] ='d';mdatBuf[6] ='a';mdatBuf[7] = 't';
        fwrite(mdatBuf, 1, 8, fp2);           

        int v_mdat_data_offset = 0;
        { //v mdat   
            std::vector<int> indexList;
            for(int i = 0; i < sampleTotalCount[0]; ++i){
                if (sampleList[0][i].offset >= slice_list[sliceIndex].start 
                   &&sampleList[0][i].offset < (slice_list[sliceIndex].start + slice_list[sliceIndex].size)){
                       indexList.push_back(i);
                }
            } 
            v_mdat_data_offset = ftell(fp2);
            for (int i = 0; i < indexList.size(); ++i){
                int sample_size = sampleList[0][indexList[i]].size;
                int sample_offset = sampleList[0][indexList[i]].offset;
                fseek(fp, sample_offset, SEEK_SET);
                int count = 0;
                while(count < sample_size){
                    fputc(fgetc(fp), fp2);
                    count++;
                }
            }        
        }
        
        int s_mdat_data_offset = 0;
        { //s mdat   
            std::vector<int> indexList;
            for(int i = 0; i < sampleTotalCount[1]; ++i){
                if (sampleList[1][i].offset >= slice_list[sliceIndex].start 
                   &&sampleList[1][i].offset < (slice_list[sliceIndex].start + slice_list[sliceIndex].size)){
                       indexList.push_back(i);
                }
            } 
            s_mdat_data_offset = ftell(fp2);
            for (int i = 0; i < indexList.size(); ++i){
                int sample_size = sampleList[1][indexList[i]].size;
                int sample_offset = sampleList[1][indexList[i]].offset;
                fseek(fp, sample_offset, SEEK_SET);
                int count = 0;
                while(count < sample_size){
                    fputc(fgetc(fp), fp2);
                    count++;
                }
            }        
        }        
        
        fileEndOffset = ftell(fp2);
        mdat_size = (fileEndOffset - mdat_start);
        set_box_size(mdat_start, (fileEndOffset - mdat_start), fp2);  
        fseek(fp2, fileEndOffset, SEEK_SET);
        
        int v_relative_offset = v_mdat_data_offset - moof_start;
        int s_relative_offset = s_mdat_data_offset - moof_start;
        
        unsigned char sizeBuf[4] = {0};
        sizeBuf[0] = (v_relative_offset & 0xff000000) >> 24;
        sizeBuf[1] = (v_relative_offset & 0x00ff0000) >> 16;
        sizeBuf[2] = (v_relative_offset & 0x0000ff00) >> 8;
        sizeBuf[3] = (v_relative_offset & 0x000000ff);
        fseek(fp2, v_trun_offset + 16, SEEK_SET);
        fwrite(sizeBuf, 1, 4, fp2);
        
        sizeBuf[0] = (s_relative_offset & 0xff000000) >> 24;
        sizeBuf[1] = (s_relative_offset & 0x00ff0000) >> 16;
        sizeBuf[2] = (s_relative_offset & 0x0000ff00) >> 8;
        sizeBuf[3] = (s_relative_offset & 0x000000ff);
        fseek(fp2, s_trun_offset + 16, SEEK_SET);
        fwrite(sizeBuf, 1, 4, fp2);        
        
        unsigned char sidx_ref_buf[12] = {0};
        int ref_size = mdat_size + moof_size;
        //printf("subsegment_duration:%d\n", subsegment_duration);
        sidx_ref_buf[0] = (ref_size & 0xff000000) >> 24;
        sidx_ref_buf[1] = (ref_size & 0x00ff0000) >> 16;
        sidx_ref_buf[2] = (ref_size & 0x0000ff00) >> 8;
        sidx_ref_buf[3] = (ref_size & 0x000000ff);
        
        sidx_ref_buf[4] = (subsegment_duration & 0xff000000) >> 24;
        sidx_ref_buf[5] = (subsegment_duration & 0x00ff0000) >> 16;
        sidx_ref_buf[6] = (subsegment_duration & 0x0000ff00) >> 8;
        sidx_ref_buf[7] = (subsegment_duration & 0x000000ff);
        
        sidx_ref_buf[8] = 0x90;
        fseek(fp2, sidex_reference_start, SEEK_SET);
        fwrite(sidx_ref_buf, 1, 12, fp2);
        
        fclose(fp2);
    }
    
    printf("generate mpd file\n");
    
    tinyxml2::XMLDocument doc;
    tinyxml2::XMLElement* MPD = doc.NewElement( "MPD" );
    MPD->SetAttribute("xmlns", "urn:mpeg:dash:schema:mpd:2011");
    MPD->SetAttribute("minBufferTime", "PT1.500S");
    MPD->SetAttribute("type", "static");
    char durationBuf[128] = {0};
    snprintf(durationBuf, sizeof(durationBuf), "PT%dH%dM%dS", media_duration_s/3600, (media_duration_s%3600)/60, media_duration_s%60);
    MPD->SetAttribute("mediaPresentationDuration", durationBuf);
    MPD->SetAttribute("maxSegmentDuration", "PT0H0M9.680S");
    MPD->SetAttribute("profiles", "urn:mpeg:dash:profile:full:2011");
    doc.LinkEndChild(MPD);
    
    tinyxml2::XMLElement* ProgramInformation = doc.NewElement( "ProgramInformation" );
    MPD->LinkEndChild(ProgramInformation);
    
    tinyxml2::XMLElement* title = doc.NewElement( "title" );
    ProgramInformation->LinkEndChild(title);    
    
    tinyxml2::XMLElement* Period = doc.NewElement( "Period" );
    Period->SetAttribute("duration", durationBuf);
    MPD->LinkEndChild(Period);
    
    tinyxml2::XMLElement*  AdaptationSet = doc.NewElement( "AdaptationSet");
    AdaptationSet->SetAttribute("segmentAlignment", "true");
    AdaptationSet->SetAttribute("maxWidth", "1280");
    AdaptationSet->SetAttribute("maxHeight", "720");
    AdaptationSet->SetAttribute("maxFrameRate", "25");
    AdaptationSet->SetAttribute("par", "19:9");
    AdaptationSet->SetAttribute("lang", "und");
    Period->LinkEndChild(AdaptationSet);
    
    tinyxml2::XMLElement*  ContentComponent = doc.NewElement( "ContentComponent");
    ContentComponent->SetAttribute("id", "1");
    ContentComponent->SetAttribute("contentType", "video");
    AdaptationSet->LinkEndChild(ContentComponent);
    
    tinyxml2::XMLElement*  ContentComponent2 = doc.NewElement( "ContentComponent");
    ContentComponent2->SetAttribute("id", "2");
    ContentComponent2->SetAttribute("contentType", "audio");    
    AdaptationSet->LinkEndChild(ContentComponent2);    
    
    tinyxml2::XMLElement*  Representation = doc.NewElement( "Representation");
    Representation->SetAttribute("id", "1");
    Representation->SetAttribute("mimeType", "video/mp4");
    Representation->SetAttribute("codecs", "avc3.4D401F,mp4a.40.2");
    Representation->SetAttribute("width", "1280");
    Representation->SetAttribute("height", "720");
    Representation->SetAttribute("frameRate", "24");
    Representation->SetAttribute("sar", "1:1");
    Representation->SetAttribute("audioSamplingRate", s_timescale);
    Representation->SetAttribute("startWithSAP", "1");
    Representation->SetAttribute("bandwidth", "1422095");
    AdaptationSet->LinkEndChild(Representation);     
    
    tinyxml2::XMLElement*  AudioChannelConfiguration = doc.NewElement( "AudioChannelConfiguration");
    AudioChannelConfiguration->SetAttribute("schemeIdUri", "urn:mpeg:dash:23003:3:audio_channel_configuration:2011");
    AudioChannelConfiguration->SetAttribute("value", "2");
    Representation->LinkEndChild(AudioChannelConfiguration);   

    tinyxml2::XMLElement*  SegmentList = doc.NewElement( "SegmentList");
    SegmentList->SetAttribute("timescale", "1000");
    SegmentList->SetAttribute("duration", slice_duration);
    Representation->LinkEndChild(SegmentList);     

    tinyxml2::XMLElement*  Initialization = doc.NewElement( "Initialization");
    Initialization->SetAttribute("sourceURL", "init.mp4");
    SegmentList->LinkEndChild(Initialization);
    
    for(int sliceIndex = 0; sliceIndex < slice_list.size(); ++sliceIndex){
        tinyxml2::XMLElement*  SegmentURL = doc.NewElement( "SegmentURL");
        char tBuf[128] = {0};
        snprintf(tBuf, sizeof(tBuf), "%08d.m4s", sliceIndex+1);
        SegmentURL->SetAttribute("media", tBuf);
        SegmentList->LinkEndChild(SegmentURL);   
    }    
    
    //doc.Print();   
    char mpdFile[1024] = {0};
    snprintf(mpdFile, sizeof(mpdFile), "%s/start.mpd", destDir);
    doc.SaveFile(mpdFile);    
    doc.Clear();
    
    for(int i = 0; i < trackCount; ++i){
        if (chunkList[i]){
            free(chunkList[i]);
            chunkList[i] = NULL;
        }
        if (sampleList[i]){
            free(sampleList[i]);
            sampleList[i] = NULL;
        }
    }
    fclose(fp);
    
    return 0;
}


