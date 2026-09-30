#ifndef __GTA_SAVEH_DIAG_H__
#define __GTA_SAVEH_DIAG_H__

// Diagnostics for SA vehicle models.
//
// The SA vehicle support has to make decisions that can only be checked in the
// running game (is the model a SA hierarchy, did the wheel meshes get cloned,
// which suspension geometry was baked into the collision model, where does the
// car actually rest). This writes those facts to "sa_vehicle.log" next to the
// executable, but only if the environment variable REVC_SA_LOG is set, so a
// normal run does not touch the disk:
//
//     set REVC_SA_LOG=1 && reVC.exe
//
// The log is opened in append mode and flushed after every line, so a crash or
// a task kill does not lose the interesting part.
#ifdef SA_VEHICLE_MODELS

#include <stdio.h>
#include <stdlib.h>

static inline FILE *SavehLogFile(void)
{
	static FILE *f = nil;
	static bool checked = false;

	if(!checked){
		const char *env = getenv("REVC_SA_LOG");
		checked = true;
		if(env && env[0] != '\0')
			f = fopen("sa_vehicle.log", "a");
	}
	return f;
}

#define SAVEH_LOG(...)					\
	do {						\
		FILE *f_ = SavehLogFile();		\
		if(f_) {				\
			fprintf(f_, __VA_ARGS__);	\
			fflush(f_);			\
		}					\
	} while(0)

#else
#define SAVEH_LOG(...) do {} while(0)
#endif

#endif
