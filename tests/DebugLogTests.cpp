#include "DebugLog.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#define change_dir _chdir
#define make_dir(path) _mkdir(path)
#define remove_dir _rmdir
#else
#include <sys/stat.h>
#include <unistd.h>
#define change_dir chdir
#define make_dir(path) mkdir(path, 0700)
#define remove_dir rmdir
#endif

static void
Check(bool condition, const char *description)
{
	if(!condition){
		fprintf(stderr, "DebugLog test failed: %s\n", description);
		exit(1);
	}
}

static std::vector<std::string>
ReadLines(const char *path)
{
	std::ifstream file(path);
	std::vector<std::string> lines;
	std::string line;
	while(std::getline(file, line))
		lines.push_back(line);
	return lines;
}

int
main(void)
{
	const char *path = "debug-log-test.log";
	remove(path);
	Check(DebugLog::Init(path), "open log");
	DebugLog::Write("INFO", "Game.cpp", 42, "Initialise", "Started %s at %d%%", "game", 100);
	DebugLog::WriteMessage("ERROR", "main.cpp", 123, "AppEventHandler", "first\nsecond\r\nthird\n");

	// Every entry is flushed, even before the normal shutdown path runs.
	std::vector<std::string> lines = ReadLines(path);
	Check(lines.size() == 4, "flushed log and multi-line messages");
	Check(lines[0].size() > 27 && lines[0][0] == '[' && lines[0][5] == '-' &&
		lines[0][8] == '-' && lines[0][11] == ' ' && lines[0][20] == '.' &&
		lines[0][24] == ']' && lines[0].find("[INFO] [Game.cpp:42 Initialise] Started game at 100%") != std::string::npos,
		"timestamp, severity, source and formatted message");
	for(size_t i = 1; i < 4; ++i)
		Check(lines[i].find("[ERROR] [main.cpp:123 AppEventHandler] ") != std::string::npos,
			"each multi-line message line is prefixed");
	Check(lines[1].find("first") != std::string::npos &&
		lines[2].find("second") != std::string::npos &&
		lines[3].find("third") != std::string::npos, "CRLF and trailing newline");

	std::string longText(6000, 'a');
	DebugLog::Write("WARNING", NULL, 0, NULL, "%s", longText.c_str());
	lines = ReadLines(path);
	Check(lines.size() == 5 && lines.back().find("[truncated]") != std::string::npos,
		"long messages are bounded and labelled");

	const int threads = 4;
	const int perThread = 60;
	std::vector<std::thread> workers;
	for(int i = 0; i < threads; ++i){
		workers.push_back(std::thread([i, perThread](){
			for(int j = 0; j < perThread; ++j)
				DebugLog::Write("DEBUG", "worker.cpp", 11, "Worker", "thread %d entry %d", i, j);
		}));
	}
	for(size_t i = 0; i < workers.size(); ++i)
		workers[i].join();
	lines = ReadLines(path);
	Check(lines.size() == 5 + threads * perThread, "all concurrent entries saved");
	bool seen[threads][perThread] = {};
	for(size_t i = 5; i < lines.size(); ++i){
		std::string::size_type pos = lines[i].find("[DEBUG] [worker.cpp:11 Worker] thread ");
		Check(pos != std::string::npos, "no interleaved or broken lines");
		int thread, entry;
		Check(sscanf(lines[i].c_str() + pos, "[DEBUG] [worker.cpp:11 Worker] thread %d entry %d", &thread, &entry) == 2 &&
			thread >= 0 && thread < threads && entry >= 0 && entry < perThread,
			"concurrent entry format");
		Check(!seen[thread][entry], "no duplicate concurrent entries");
		seen[thread][entry] = true;
	}

	Check(make_dir("debug-log-test-subdir") == 0, "create temporary directory");
	Check(change_dir("debug-log-test-subdir") == 0, "change working directory");
	DebugLog::WriteMessage("INFO", NULL, 0, NULL, "still in the original file");
	Check(change_dir("..") == 0, "restore working directory");
	Check(remove_dir("debug-log-test-subdir") == 0, "remove temporary directory");
	lines = ReadLines(path);
	Check(lines.size() == 6 + threads * perThread &&
		lines.back().find("still in the original file") != std::string::npos,
		"logging after chdir");

	DebugLog::Shutdown();
	DebugLog::WriteMessage("INFO", NULL, 0, NULL, "must not appear");
	Check(DebugLog::Init("nonexistent-log-test-subdir/debug.log") == false,
		"failed open is reported instead of crashing");
	Check(DebugLog::Init(path), "can retry after failed open");
	DebugLog::WriteMessage("INFO", NULL, 0, NULL, "new session");
	DebugLog::Shutdown();
	lines = ReadLines(path);
	Check(lines.size() == 1 && lines[0].find("new session") != std::string::npos,
		"new session truncates old log");
	remove(path);
	return 0;
}
