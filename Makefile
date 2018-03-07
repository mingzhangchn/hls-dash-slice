

SRC=mp4_slice.cpp tinyxml2.cpp test.cpp ts_slice.cpp
OBJ=mp4_slice.o tinyxml2.o test.o ts_slice.o

APP=test
LIB=libmt_slice.so

INCLUDES = -I./

.PONEY:all

all:$(APP) $(LIB)


$(APP):$(OBJ) 
	@g++  $(OBJ) -o $@ 
 
$(LIB):$(OBJ)
	g++ -shared -o $(LIB) mp4_slice.o tinyxml2.o ts_slice.o
    
%.o:%.cpp
	g++ -c  -fPIC $(INCLUDES) $< -o $@
	
%.o:%.c
	gcc -c $(INCLUDES) $< -o $@    
    
clean:
	@rm -rf $(OBJ) $(APP)
	
	