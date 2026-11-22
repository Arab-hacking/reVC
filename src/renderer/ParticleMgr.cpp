#include "common.h"

#include "main.h"
#include "FileMgr.h"
#include "ParticleMgr.h"
#ifdef CUSTOM_MODELS
#include <vector>
#include "CustomModels.h"
#endif

cParticleSystemMgr mod_ParticleSystemManager;

const char *ParticleFilename = "PARTICLE.CFG";

cParticleSystemMgr::cParticleSystemMgr()
{
	memset(this, 0, sizeof(*this));
}

void cParticleSystemMgr::Initialise()
{
	LoadParticleData();
	
	for ( int32 i = 0; i < MAX_PARTICLES; i++ )
		m_aParticles[i].m_pParticles = nil;
}

// SA cParticleSystemMgr::LoadParticleData (0x5BE070).
//
// The table is a fixed set of tab separated columns per particle type, read in
// order, so the parser is positional rather than keyed. It used to index
// straight into work_buff without ever checking that the file had been read:
// the archive supplies data/particle.cfg and the VC folder does not have to
// have a DATA\PARTICLE.CFG at all, so a missing file left work_buff full of
// zeroes and this loop walked off the end of it looking for a newline. Hence
// the assertion at lineLength < 500 instead of a particle table.
void cParticleSystemMgr::LoadParticleData()
{
	// Same two sources, in the same order, as every other data table in this
	// build: the archive first, the loose folder as the fallback.
	char *data = nil;
	int32 size = 0;

#ifdef CUSTOM_MODELS
	std::vector<uint8> archived;
	if(CCustomModels::ReadCommonDataFile("data/particle.cfg", archived) && !archived.empty()){
		archived.push_back('\0');
		data = (char *)archived.data();
		size = (int32)archived.size() - 1;
	}
#endif

	if(data == nil){
		CFileMgr::SetDir("DATA");
		ssize_t loaded = CFileMgr::LoadFile(ParticleFilename, work_buff, ARRAY_SIZE(work_buff) - 1, "r");
		CFileMgr::SetDir("");

		if(loaded <= 0){
			REVC_LOG_ERROR("particle data: %s is missing; every particle keeps the defaults",
			               ParticleFilename);
			return;
		}
		work_buff[loaded] = '\0';
		data = (char *)work_buff;
		size = (int32)loaded;
	}

	tParticleSystemData *entry = nil;
	int32 type = PARTICLE_FIRST;

	char *lineStart = data;
	char *end = data + size;
	
	char line[500];
	char delims[4];

	// One pass per line. The terminator is the newline, the end of the data, or
	// the terminator byte LoadFile wrote - whichever comes first, so a file
	// without a trailing newline does not run past its own buffer.
	while ( lineStart < end )
	{
		char *lineEnd = lineStart;
		while ( lineEnd < end && *lineEnd != '\n' )
			++lineEnd;
		
		int32 lineLength = lineEnd - lineStart;
#ifdef GTA_PS2
		// CRs are left untouched. we assume every line ends in one
		lineLength--;
#endif

		if(lineLength >= (int32)sizeof(line)){
			REVC_LOG_WARNING("particle data: line longer than %d characters, skipped",
			                 (int32)sizeof(line) - 1);
			lineStart = lineEnd + 1;
			continue;
		}

		// Files checked out on Windows carry CRLF, and the terminator check
		// below is an exact compare, so the CR has to go before it.
		while ( lineLength > 0 && (lineStart[lineLength - 1] == '\r' || lineStart[lineLength - 1] == ' ') )
			lineLength--;

		strncpy(line, lineStart, lineLength);
		
		line[lineLength] = '\0';
		
		if ( !strcmp(line, ";the end") )
			break;
		
		if ( *line != ';' )
		{
			int32 param = CFG_PARAM_FIRST;

			strcpy(delims, " \t");
			
			char *value = strtok(line, delims);
			
			ASSERT(value != nil);
			
			do
			{	
				// A line that does not start with a particle name has nothing
				// to attach to. SA indexes m_aParticles blindly because its
				// file always starts each line with a name; this build reads a
				// table it did not ship, so it checks.
				if ( entry == nil && param != CFG_PARAM_PARTICLE_TYPE_NAME )
					break;

				switch ( param )
				{
					case CFG_PARAM_PARTICLE_TYPE_NAME:
						if(type >= MAX_PARTICLES){
							REVC_LOG_WARNING("particle data: more than %d particle types, the rest is ignored",
							                 (int32)MAX_PARTICLES);
							return;
						}
						entry = &m_aParticles[type];
						ASSERT(entry != nil);
						entry->m_Type = (tParticleType)type++;
						strncpy(entry->m_aName, value, sizeof(entry->m_aName) - 1);
						entry->m_aName[sizeof(entry->m_aName) - 1] = '\0';
						break;

					case CFG_PARAM_RENDER_COLOURING_R:
						entry->m_RenderColouring.red = atoi(value);
						break;

					case CFG_PARAM_RENDER_COLOURING_G:
						entry->m_RenderColouring.green = atoi(value);
						break;

					case CFG_PARAM_RENDER_COLOURING_B:
						entry->m_RenderColouring.blue = atoi(value);
						break;

					case CFG_PARAM_INITIAL_COLOR_VARIATION:
						entry->m_InitialColorVariation = Min(atoi(value), 100);
						break;

					case CFG_PARAM_FADE_DESTINATION_COLOR_R:
						entry->m_FadeDestinationColor.red = atoi(value);
						break;

					case CFG_PARAM_FADE_DESTINATION_COLOR_G:
						entry->m_FadeDestinationColor.green = atoi(value);
						break;

					case CFG_PARAM_FADE_DESTINATION_COLOR_B:
						entry->m_FadeDestinationColor.blue = atoi(value);
						break;

					case CFG_PARAM_COLOR_FADE_TIME:
						entry->m_ColorFadeTime = atoi(value);
						break;

					case CFG_PARAM_DEFAULT_INITIAL_RADIUS:
						entry->m_fDefaultInitialRadius = atof(value);
						break;

					case CFG_PARAM_EXPANSION_RATE:
						entry->m_fExpansionRate = atof(value);
						break;

					case CFG_PARAM_INITIAL_INTENSITY:
						entry->m_nFadeToBlackInitialIntensity = atoi(value);
						break;

					case CFG_PARAM_FADE_TIME:
						entry->m_nFadeToBlackTime = atoi(value);
						break;

					case CFG_PARAM_FADE_AMOUNT:
						entry->m_nFadeToBlackAmount = atoi(value);
						break;

					case CFG_PARAM_INITIAL_ALPHA_INTENSITY:
						entry->m_nFadeAlphaInitialIntensity = atoi(value);
						break;

					case CFG_PARAM_FADE_ALPHA_TIME:
						entry->m_nFadeAlphaTime = atoi(value);
						break;

					case CFG_PARAM_FADE_ALPHA_AMOUNT:
						entry->m_nFadeAlphaAmount = atoi(value);
						break;

					case CFG_PARAM_INITIAL_ANGLE:
						entry->m_nZRotationInitialAngle = atoi(value);
						break;

					case CFG_PARAM_CHANGE_TIME:
						entry->m_nZRotationChangeTime = atoi(value);
						break;

					case CFG_PARAM_ANGLE_CHANGE_AMOUNT:
						entry->m_nZRotationAngleChangeAmount = atoi(value);
						break;

					case CFG_PARAM_INITIAL_Z_RADIUS:
						entry->m_fInitialZRadius = atof(value);
						break;

					case CFG_PARAM_Z_RADIUS_CHANGE_TIME:
						entry->m_nZRadiusChangeTime = atoi(value);
						break;

					case CFG_PARAM_Z_RADIUS_CHANGE_AMOUNT:
						entry->m_fZRadiusChangeAmount = atof(value);
						break;

					case CFG_PARAM_ANIMATION_SPEED:
						entry->m_nAnimationSpeed = atoi(value);
						break;

					case CFG_PARAM_START_ANIMATION_FRAME:
						entry->m_nStartAnimationFrame = atoi(value);
						break;

					case CFG_PARAM_FINAL_ANIMATION_FRAME:
						entry->m_nFinalAnimationFrame = atoi(value);
						break;

					case CFG_PARAM_ROTATION_SPEED:
						entry->m_nRotationSpeed = atoi(value);
						break;

					case CFG_PARAM_GRAVITATIONAL_ACCELERATION:
						entry->m_fGravitationalAcceleration = atof(value);
						break;

					case CFG_PARAM_FRICTION_DECCELERATION:
						entry->m_nFrictionDecceleration = atoi(value);
						break;

					case CFG_PARAM_LIFE_SPAN:
						entry->m_nLifeSpan = atoi(value);
						break;

					case CFG_PARAM_POSITION_RANDOM_ERROR:
						entry->m_fPositionRandomError = atof(value);
						break;

					case CFG_PARAM_VELOCITY_RANDOM_ERROR:
						entry->m_fVelocityRandomError = atof(value);
						break;

					case CFG_PARAM_EXPANSION_RATE_ERROR:
						entry->m_fExpansionRateError = atof(value);
						break;

					case CFG_PARAM_ROTATION_RATE_ERROR:
						entry->m_nRotationRateError = atoi(value);
						break;

					case CFG_PARAM_LIFE_SPAN_ERROR_SHAPE:
						entry->m_nLifeSpanErrorShape = atoi(value);
						break;

					case CFG_PARAM_TRAIL_LENGTH_MULTIPLIER:
						entry->m_fTrailLengthMultiplier = atof(value);
						break;
						
					case CFG_PARAM_STRETCH_VALUE_X:
						entry->m_vecTextureStretch.x = atof(value);
						break;

					case CFG_PARAM_STRETCH_VALUE_Y:
						entry->m_vecTextureStretch.y = atof(value);
						break;

					case CFG_PARAM_WIND_FACTOR:
						entry->m_fWindFactor = atof(value);
						break;

					case CFG_PARAM_PARTICLE_CREATE_RANGE:
						entry->m_fCreateRange = SQR(atof(value));
						break;

					case CFG_PARAM_FLAGS:
						entry->Flags = atoi(value);
						break;
				}
				
				value = strtok(nil, delims);

				param++;

				if ( param > CFG_PARAM_LAST )
					param = CFG_PARAM_FIRST;
		
			} while ( value != nil );
		}
		
		// Next line. A file whose last line has no newline simply ends here.
		lineStart = lineEnd < end ? lineEnd + 1 : end;
	}
}
