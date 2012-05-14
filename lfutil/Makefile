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

DEF_FLAGS = -D_REENTRANT -DLINUX $(DBG_FLAGS) -DDEBUG_LOCK
#DEF_FLAGS = -D_REENTRANT -DLINUX $(DBG_FLAGS) -DMEM_DEBUG -DDEBUG_LOCK

COMPILE_FLAGS = -Wall -Werror -Wno-non-virtual-dtor -fPIC

CFLAGS += $(INC_FLAGS)  $(DEF_FLAGS) $(COMPILE_FLAGS)
CXXFLAGS = $(CFLAGS)


MYLIB      = liblutil.so
LIB_LINKLIBS = -shared -ldl -lpthread -lrt

TEST_BIN  = ltest
TEST_CPP  =  $(foreach dir, $(CUR_DIR), $(shell cd $(CUR_DIR); ls -1 test/*.cpp)) 
TEST_OBJ  =  $(addprefix $(TARGET_DIR)/, $(TEST_CPP:.cpp=.o))

default: makedir $(MYLIB)


$(MYLIB): $(COMP_OBJ)
	$(CXX) $^ $(LIB_LINKLIBS) -o $@


$(TEST_BIN): makedir $(MYLIB) $(TEST_OBJ)
	$(CXX) $(TEST_OBJ) -L$(CUR_DIR) -llutil -o $@

$(TARGET_DIR)/%.o : src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@ 

#test obj
$(TARGET_DIR)/%.o : %.cpp
	$(CXX) $(CXXFLAGS) -Itest -c $< -o $@ 

$(TARGET_DIR)/%.o : src/%.c
	$(CC) $(CFLAGS) -c $< -o $@ 
    
#%. : %.o
#   $(CXX) $^ $(LINKLIBS) -o $@


.PHONY: makedir
makedir:
	@mkdir -p $(TARGET_DIR)
	@mkdir -p $(TARGET_SUB_DIR)
	@mkdir -p $(TARGET_DIR)/test


.PHONY: clean
clean:
	@rm -rf $(TARGET_DIR) 
	@rm -f $(MYLIB) $(TEST_BIN)

.PHONY: install
	@echo "no install right now"

macro:
	@echo TARGET_DIR $(TARGET_DIR)
	@echo CUR_DIR $(CUR_DIR) 
	@echo COMP_CXX $(COMP_CXX)
	@echo SUB_DIR $(SUB_DIR)
	@echo SUB_DIR_OBJ $(SUB_DIR_OBJ)
	@echo SUB_DIR_INC $(SUB_DIR_INC)
	@echo TARGET_SUB_DIR $(TARGET_SUB_DIR)
	@echo CPP_FILE $(CPP_FILE)
	@echo COMP_OBJ $(COMP_OBJ)
	@echo CFLAGS $(CFLAGS) 
	@echo LIB_LINKLIBS $(LIB_LINKLIBS)
	@echo TEST_CPP $(TEST_CPP)
	@echo TEST_OBJ $(TEST_OBJ)
