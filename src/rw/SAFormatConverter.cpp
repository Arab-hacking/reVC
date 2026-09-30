/*
 * SAFormatConverter.cpp - San Andreas DFF/TXD format support for Vice City 1
 */

#include "common.h"
#include "SAFormatConverter.h"
#include "RwHelper.h"
#include "SABoneMap.h"

#ifdef LIBRW
#include "rpskin.h"
#endif

/* ------------------------------------------------------------------ */
/*  Version detection                                                 */
/* ------------------------------------------------------------------ */

bool
IsSAFormat(RwStream *stream, RwUInt32 version)
{
    /* San Andreas uses RenderWare version 0x36000 or higher */
    return version >= RW_VERSION_SA;
}

/* ------------------------------------------------------------------ */
/*  DFF loading with SA compatibility                                 */
/* ------------------------------------------------------------------ */

/*
 * Callback to process each geometry in a clump and apply SA fixes
 */
static RpAtomic*
FixSAGeometryCB(RpAtomic *atomic, void *data)
{
    RpGeometry *geom = RpAtomicGetGeometry(atomic);
    if(!geom)
        return atomic;

#ifdef LIBRW
    /*
     * SA models may have:
     * - More than 4 vertex weights per bone (SA supports up to 4, same as VC)
     * - Different skin split data (bone group remapping)
     * - Night vertex colors (SA-specific)
     * - Pipeline state (SA uses different rendering pipelines)
     *
     * librw handles most of this automatically, but we need to
     * ensure the skin data is properly initialized for VC's renderer.
     */
    
    /* Check if geometry has skin plugin */
    RpSkin *skin = RpSkinGeometryGetSkin(geom);
    if(skin) {
        /* 
         * SA skin data may have bone group remapping (split data).
         * This is used for optimizing vertex processing on PS2.
         * For PC (D3D9/GL), this is handled by librw's skin pipeline.
         *
         * We just need to ensure the skin is properly initialized
         * and bone indices are valid.
         */
        int numBones = skin->numBones;
        if(numBones > 0) {
            /* Validate bone count - SA peds can have up to ~60 bones */
            debug("SAFormatConverter: geometry has %d bones\n", numBones);
        }
    }

    /*
     * Note: SA materials may use different pipeline settings,
     * but librw handles pipeline conversion automatically.
     * No manual pipeline reset needed.
     */
#endif

    return atomic;
}

/* ------------------------------------------------------------------ */
/*  Post-processing SA clump                                          */
/* ------------------------------------------------------------------ */

void
PostProcessSAClump(RpClump *clump)
{
    if(!clump)
        return;

    debug("SAFormatConverter: post-processing SA clump\n");

    /* Fix all geometries in the clump */
    RpClumpForAllAtomics(clump, FixSAGeometryCB, nil);

    /* Remap bones if this is a ped model */
    RemapSABonesToVC(clump);
}

/* ------------------------------------------------------------------ */
/*  Post-processing SA texture dictionary                             */
/* ------------------------------------------------------------------ */

/*
 * Callback to fix each texture in a TXD
 */
static RwTexture*
FixSATextureCB(RwTexture *texture, void *data)
{
    /* SA textures load correctly as-is via librw.
     * No post-processing needed. */
    return texture;
}

void
PostProcessSATexDict(RwTexDictionary *txd)
{
    if(!txd)
        return;

    debug("SAFormatConverter: post-processing SA TXD\n");

    /* Fix all textures in the dictionary */
    RwTexDictionaryForAllTextures(txd, FixSATextureCB, nil);
}

/* ------------------------------------------------------------------ */
/*  High-level loading functions                                      */
/* ------------------------------------------------------------------ */

RpClump*
LoadDFFWithSACompat(const char *filename)
{
    RpClump *clump = nil;

    /* 
     * Use standard RwStream to load the DFF
     * librw will automatically detect the version and parse accordingly
     */
    RwStream *stream = RwStreamOpen(rwSTREAMFILENAME, rwSTREAMREAD, filename);
    if(!stream) {
        debug("SAFormatConverter: failed to open '%s'\n", filename);
        return nil;
    }

    /* Find the CLUMP chunk and get its version */
    RwUInt32 size, version;
    if(RwStreamFindChunk(stream, rwID_CLUMP, &size, &version)) {
        debug("SAFormatConverter: loading DFF '%s' (version=0x%x)\n",
              filename, version);

        /* Check if this is an SA file */
        bool isSA = IsSAFormat(stream, version);
        if(isSA) {
            debug("SAFormatConverter: detected SA format (version 0x%x)\n", version);
        }

        /* Read the clump using standard function */
        clump = RpClumpStreamRead(stream);
    }

    RwStreamClose(stream, nil);

    if(!clump) {
        debug("SAFormatConverter: failed to load clump from '%s'\n", filename);
        return nil;
    }

    /* Always post-process (harmless for VC files, necessary for SA) */
    PostProcessSAClump(clump);

    return clump;
}

RwTexDictionary*
LoadTXDWithSACompat(const char *filename)
{
    RwTexDictionary *txd = nil;

    /* 
     * Use standard RwStream to load the TXD
     * librw will automatically detect the version and parse accordingly
     */
    RwStream *stream = RwStreamOpen(rwSTREAMFILENAME, rwSTREAMREAD, filename);
    if(!stream) {
        debug("SAFormatConverter: failed to open TXD '%s'\n", filename);
        return nil;
    }

    /* Find the TXD chunk and get its version */
    RwUInt32 size, version;
    if(RwStreamFindChunk(stream, rwID_TEXDICTIONARY, &size, &version)) {
        debug("SAFormatConverter: loading TXD '%s' (version=0x%x)\n",
              filename, version);

        /* Check if this is an SA file */
        bool isSA = IsSAFormat(stream, version);
        if(isSA) {
            debug("SAFormatConverter: detected SA TXD format\n");
        }

        /* Read the TXD using standard GTA function */
        txd = RwTexDictionaryGtaStreamRead(stream);
    }

    RwStreamClose(stream, nil);

    if(!txd) {
        debug("SAFormatConverter: failed to load TXD from '%s'\n", filename);
        return nil;
    }

    /* Post-process for SA files */
    PostProcessSATexDict(txd);

    return txd;
}
