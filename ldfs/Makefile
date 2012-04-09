# __CR__
# Copyright (c) 2008-2010 Longda Corporation  All Rights Reserved
# This software contains the intellectual property of Longda Corporation
# or is licensed to Longda Corporation from third parties.  
# Use of this software and the intellectual property 
# contained therein is expressly limited to the terms 
# and conditions of the License Agreement under 
# which  it is provided by or on behalf of Longda.
# __CR__
#################################################

include common.def

ifeq ($(DEBUG),1)
	DBG_FLAGS = -g -DDEBUG -O0
else
	DBG_FLAGS = -g -O2
endif
CXXFLAGS += $(DBG_FLAGS)
CXXFLAGS += -fPIC

#Default _POSIX_NAME_MAX=14
#DEF_FLAGS = -DDEBUG_LOCK_LOG -D_POSIX_NAME_MAX=256 -DLINUX -DHAVE_CONFIG_H -D_FILE_OFFSET_BITS=64 -D_REENTRANT 
DEF_FLAGS = -D_POSIX_NAME_MAX=256 -DLINUX -DHAVE_CONFIG_H -D_FILE_OFFSET_BITS=64 -D_REENTRANT 
CXXFLAGS += $(DEF_FLAGS)

SHLDFLAGS=-m64
PRODINC  = -I. -Iinclude/common/seda -Iinclude/afuse -Iinclude/common -Iinclude -Iinclude/fuse \
			-I/usr/local/include -I/usr/local/include/fuse -I/usr/local/include/fastcommon \
			-I/usr/local/include/fastdfs -I/usr/local/include/fastdht -I/usr/include/linux
PRODLIBS = -L/usr/local/lib -L /usr/lib64 -lpthread -lfuse \
			-lrt -ldl -lfdhtclient -lfdfsclient -lfastcommon

PRODDEST  = afuse
CUR_DIR  := $(AFUSE_DIR)
SUB_DIR  := src/afuse src/common src/common/seda
SRC_CPP   = $(foreach dir, $(SUB_DIR), $(shell cd $(CUR_DIR); ls -1 $(dir)/*.cpp))
OBJ_CPP   = $(SRC_CPP:.cpp=.o)

CFG_DIR       = etc

default: start $(PRODDEST)

start:
	@echo "**************make ${PRODDEST}***************************"

.PHONY:${PRODDEST}
${PRODDEST}:${OBJ_CPP}
	$(CXX) $(OBJ_CPP) $(SHLDFLAGS) ${PRODLIBS} -o ${PRODDEST}	
	
${OBJ_CPP} : %.o : %.cpp    
	$(CXX) $(CXXFLAGS) $(SHLDFLAGS) -c $< ${PRODINC} -o $@ 

clean:
	-rm -f src/*/*.o ${PRODDEST}

install: start $(PRODDEST) 
	#$(INSTALL) $(INST_BIN_FLAGS) $(PRODDEST) $(EBILL_BIN_DIR)
	#$(CP) $(CFG_DIR) $(EBILL_BIN_DIR)
	#$(MKDIR) -p $(EBILL_BIN_DIR)/logs
	@echo "Not implement right now"


