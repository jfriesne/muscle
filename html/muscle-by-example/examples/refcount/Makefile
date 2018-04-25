EXECUTABLES = \
	example_1_basic_usage      \
	example_2_with_object_pool \
	example_3_conversions      \
	example_4_dummy_refs

MUSCLE_DIR = ../../../..
MUSCLE_LIB = $(MUSCLE_DIR)/server/libmuscle.a

CXXFLAGS   = -I$(MUSCLE_DIR) -std=c++11 -O3 -DMUSCLE_ENABLE_ZLIB_ENCODING
LFLAGS    += -L$(MUSCLE_DIR)/server
OBJS       = $(EXECUTABLES:=.o)

ifeq ($(OSTYPE),)
  OSTYPE = $(strip $(shell uname))
endif

ifeq ($(OSTYPE),Darwin) 
   LIBS += -framework IOKit -framework Carbon -framework SystemConfiguration
   CXXFLAGS += -stdlib=libc++
endif 

ifeq ($(OSTYPE),linux) 
   LIBS += -lutil -pthread
endif 
ifeq ($(OSTYPE),Linux) 
   LIBS += -lutil -pthread
endif 

all:  $(MUSCLE_LIB) $(EXECUTABLES)

$(MUSCLE_LIB):
	cd $(MUSCLE_DIR)/server && make

$(EXECUTABLES) : % : $(MUSCLE_LIB) $(OBJS)
	$(CXX) $(CXXFLAGS) $(LFLAGS) -o $@ $@.cpp $(MUSCLE_LIB) $(LIBS)

clean:
	rm -f *.o *.xSYM $(EXECUTABLES)
