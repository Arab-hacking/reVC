#include "common.h"

#include <string>
#include <vector>

#include "CustomZip.h"

static bool
ReadExpected(const std::string &path, std::vector<uint8> &bytes)
{
	FILE *file = fopen(path.c_str(), "rb");
	long size;
	if(file == nil)
		return false;
	if(fseek(file, 0, SEEK_END) != 0){
		fclose(file);
		return false;
	}
	size = ftell(file);
	if(size < 0 || fseek(file, 0, SEEK_SET) != 0){
		fclose(file);
		return false;
	}
	bytes.resize((size_t)size);
	bool ok = bytes.empty() || fread(bytes.data(), 1, bytes.size(), file) == bytes.size();
	fclose(file);
	return ok;
}

int
main(int argc, char **argv)
{
	CustomZip zip;
	int i, checked = 0;
	if(argc != 3){
		fprintf(stderr, "usage: %s <archive.zip> <expected-directory>\n", argv[0]);
		return 2;
	}
	if(!zip.Open(argv[1])){
		fprintf(stderr, "failed to open %s\n", argv[1]);
		return 1;
	}

	for(i = 0; i < zip.GetNumEntries(); i++){
		const CustomZipEntry *entry = zip.GetEntry(i);
		std::string expectedPath = argv[2];
		std::vector<uint8> expected, actual;
		if(entry == nil || entry->uncompressedSize > CUSTOMZIP_MAX_ENTRY){
			fprintf(stderr, "invalid test entry at index %d\n", i);
			return 1;
		}
		expectedPath += "/";
		expectedPath += entry->name;
		if(!ReadExpected(expectedPath, expected)){
			fprintf(stderr, "missing expected output for %s\n", entry->name);
			return 1;
		}
		if(expected.size() != entry->uncompressedSize){
			fprintf(stderr, "size mismatch for %s: expected %lu, archive says %llu\n",
				entry->name, (unsigned long)expected.size(),
				(unsigned long long)entry->uncompressedSize);
			return 1;
		}
		actual.resize(expected.size());
		if(!zip.Extract(i, actual.empty() ? nil : actual.data(), (uint32)actual.size())){
			fprintf(stderr, "extract failed for %s (method %u)\n", entry->name, entry->method);
			return 1;
		}
		if(!actual.empty() && memcmp(actual.data(), expected.data(), actual.size()) != 0){
			fprintf(stderr, "content mismatch for %s\n", entry->name);
			return 1;
		}
		checked++;
	}
	printf("PASS: extracted and verified %d ZIP entries from %s\n", checked, argv[1]);
	return checked == 0 ? 1 : 0;
}
