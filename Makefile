CFLAGS = -c -g -Wall -std=gnu99 `pkg-config fuse --cflags --libs`

LDFLAGS = `pkg-config fuse --cflags --libs`

# Uncomment one of the following three lines to compile
#SOURCES= sfs_util.c root_dir_cache.c disk_emu.c sfs_api.c sfs_test.c sfs_api.h 
SOURCES= sfs_util.c root_dir_cache.c disk_emu.c sfs_api.c sfs_test2.c sfs_api.h
#SOURCES= sfs_util.c root_dir_cache.c disk_emu.c sfs_api.c fuse_wrappers.c sfs_api.h

OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=braedon_mcdonald_sfs

all: $(SOURCES) $(HEADERS) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	gcc $(OBJECTS) $(LDFLAGS) -o $@ -lm

.c.o:
	gcc $(CFLAGS) $< -o $@

clean:
	rm -rf *.o *~ $(EXECUTABLE)
