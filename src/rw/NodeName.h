#ifndef __GTA_NODENAME_H__
#define __GTA_NODENAME_H__

bool NodeNamePluginAttach(void);
char *GetFrameNodeName(RwFrame *frame);
void SetFrameNodeName(RwFrame *frame, const char *name);

#endif // __GTA_NODENAME_H__
