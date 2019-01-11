#include <vitasdk.h>
#include <stdlib.h>
#include <stdio.h>
#include "hbTrophy.h"
#include "tinyxml2.h"
using namespace tinyxml2;

#define LARGE_SCOPE_MIN_SCORE 950
#define SMALL_SCOPE_MIN_SCORE 285

uint32_t Endian_UInt32_Conversion(uint32_t value) {
	return ((value >> 24) & 0x000000FF) | ((value >> 8) & 0x0000FF00) | ((value << 8) & 0x00FF0000) | ((value << 24) & 0xFF000000);
}

uint64_t Endian_UInt64_Conversion(uint64_t value) {
	return ((value >> 56) & 0x00000000000000FF) | ((value >> 40) & 0x000000000000FF00) | ((value >> 24) & 0x0000000000FF0000) | ((value >> 8) & 0x00000000FF000000) | ((value << 8) & 0x000000FF00000000) | ((value << 24) & 0x0000FF00000000) | ((value << 40) & 0x00FF0000000000) | ((value << 56) & 0xFF000000000000);
}

static int lib_initialized = 0;

typedef struct trophyCommand {
	uint32_t type;
	uint32_t data;
	uint32_t result;
} trophyCommand;

typedef struct trophyContext {
	char titleid[10];
	SceUID in_mutex;
	SceUID out_mutex;
	SceUID thread;
	trophyCommand cmd;
	int trophy_num;
	bool has_platinum;
} trophyContext;

static void copyDir(const char *src, const char *dst) {
	sceIoMkdir(dst, 0777);
	char src_fname[256];
	char dst_fname[256];
	SceUID d = sceIoDopen(src);
	SceIoDirent entry;
	while (sceIoDread(d, &entry) > 0) {
		sprintf(src_fname, "%s/%s", src, entry.d_name);
		sprintf(dst_fname, "%s/%s", dst, entry.d_name);
		if SCE_S_ISDIR(entry.d_stat.st_mode) {
			copyDir(src_fname, dst_fname);
		} else {
			sceIoRename(src_fname, dst_fname);
		}
	}
	sceIoDclose(d);
	sceIoRmdir(src);
}

static void delDir(const char *src) {
	char src_fname[256];
	SceUID d = sceIoDopen(src);
	SceIoDirent entry;
	while (sceIoDread(d, &entry) > 0) {
		sprintf(src_fname, "%s/%s", src, entry.d_name);
		if SCE_S_ISDIR(entry.d_stat.st_mode) {
			delDir(src_fname);
		} else {
			sceIoRemove(src_fname);
		}
	}
	sceIoDclose(d);
	sceIoRmdir(src);
}

static int trophyThread(unsigned int argc, void* argv) {
	
	// Getting trophy context
	trophyContext *ctx = (trophyContext*)argv;
	
	for (;;) {
		
		// Waiting for a command request
		sceKernelWaitSema(ctx->in_mutex, 1, NULL);
		
		// Sending command result
		ctx->cmd.result = HB_TROPHY_ERROR_OK;
		sceKernelSignalSema(ctx->out_mutex, 1);
		
	}
	
	return HB_TROPHY_ERROR_OK;
}

int hbTrophyInit(void *opt) {
	// Initializing hbTrophy lib
	if (lib_initialized) return HB_TROPHY_ERROR_ALREADY_INITIALIZED;
	lib_initialized = 1;
	sceIoMkdir("ux0:data/hbTrophy", 0777);
	return HB_TROPHY_ERROR_OK;
}

int hbTrophyTerm(void) {
	// Terminating hbTrophy lib
	if (!lib_initialized) return HB_TROPHY_ERROR_NOT_INITIALIZED;
	lib_initialized = 0;
	return HB_TROPHY_ERROR_OK;
}

int hbTrophyCreateContext(hbTrophyContext *context, hbTrophyCommunicationId *id, hbTrophyCommunicationSignature *sign, uint64_t options) {
	// Checking for lib being initialized
	if (!lib_initialized) return HB_TROPHY_ERROR_NOT_INITIALIZED;
	char tmp[32];
	sceIoMkdir("ux0:data/hbTrophy/tmp", 0777);
	
	// Extracting trophy list file
	char filename[256];
	uint32_t filecount;
	uint32_t filestart;
	sprintf(filename, "app0:sce_sys/trophy/%s_00/TROPHY.TRP", id->data);
	FILE *f = fopen(filename, "rb");
	if (f == NULL) return HB_TROPHY_ERROR_INVALID_COMM_ID;
	fseek(f, 0x10, SEEK_SET);
	fread(&filecount, 1, 4, f);
	fread(&filestart, 1, 4, f);
	filecount = Endian_UInt32_Conversion(filecount);
	filestart = Endian_UInt32_Conversion(filestart);
	int i;
	for (i = 0; i < filecount; i++) {
		uint64_t offset;
		uint64_t size;
		memset(tmp, 0, 0x20);
		fseek(f, filestart + i * 0x40, SEEK_SET);
		fread(tmp, 1, 0x20, f);
		fread(&offset, 1, 8, f);
		offset = Endian_UInt64_Conversion(offset);
		fread(&size, 1, 8, f);
		size = Endian_UInt64_Conversion(size);
		fseek(f, offset, SEEK_SET);
		sprintf(filename, "ux0:data/hbTrophy/tmp/%s", tmp);
		FILE *out = fopen(filename, "wb");
		uint8_t *data = (uint8_t*)malloc(size);
		fread(data, 1, size, f);
		fwrite(data, 1, size, out);
		fclose(out);
		free(data);
	}
	fclose(f);
	
	// Checking if trophy list is already installed and its versioning
	sprintf(filename, "ux0:data/hbTrophy/%s/TROPCONF.SFM", id->data);
	f = fopen(filename, "rb");
	float version = 0.0f;
	if (f != NULL) {
		XMLDocument doc;
		doc.LoadFile(f);
		XMLNode *trophyconf = doc.FirstChildElement("trophyconf");
		XMLElement *set_version = trophyconf->FirstChildElement("trophyset-version");
		set_version->QueryFloatText(&version);
		fclose(f);
	}
	
	// Parse TROPCONF.SFM
	float new_version = 0.0f;
	XMLDocument doc;
	doc.LoadFile("ux0:data/hbTrophy/tmp/TROPCONF.SFM");
	auto trophyconf = doc.FirstChildElement("trophyconf");
	auto set_version = trophyconf->FirstChildElement("trophyset-version");
	set_version->QueryFloatText(&new_version);
	
	// Updating installed trophy list if version is newer
	sprintf(filename, "ux0:data/hbTrophy/%s", id->data);
	if (new_version > version) {
		copyDir("ux0:data/hbTrophy/tmp", filename);
	} else {
		delDir("ux0:data/hbTrophy/tmp");
	}
	
	// Creating internal trophy context
	trophyContext *ctx = (trophyContext*)malloc(sizeof(trophyContext));
	if (ctx == NULL) return HB_TROPHY_ERROR_OUT_OF_MEMORY;
	memcpy(ctx->titleid, id->data, 9);
	ctx->titleid[10] = 0;
	
	// Populating context
	sprintf(filename, "ux0:data/hbTrophy/%s/TROPCONF.SFM", ctx->titleid);
	doc.LoadFile("ux0:data/hbTrophy/tmp/TROPCONF.SFM");
	trophyconf = doc.FirstChildElement("trophyconf");
	if (strcmp(trophyconf->Attribute("policy"), "large") == 0) ctx->has_platinum = true;
	else ctx->has_platinum = false;
	
	// Integrity Checks
	int tot_score = 0;
	ctx->trophy_num = 0;
	bool plat_found = false;
	auto trophy = trophyconf->FirstChildElement("trophy");
	do{
		ctx->trophy_num++;
		const char *type = trophy->Attribute("ttype");
		switch (type[0]){
		case 'P':
			if ((!ctx->has_platinum) || plat_found) {
				free(ctx);
				return HB_TROPHY_ERROR_INVALID_TRP_LIST;
			}
			plat_found = true;
			break;
		case 'G':
			tot_score += 90;
			break;
		case 'S':
			tot_score += 30;
			break;
		case 'B':
			tot_score += 15;
			break;
		}
		trophy = trophy->NextSiblingElement("trophy");
	}while (trophy != nullptr);
	if ((ctx->has_platinum && (!plat_found)) || (ctx->has_platinum && (tot_score < LARGE_SCOPE_MIN_SCORE)) || ((!ctx->has_platinum) && (tot_score < SMALL_SCOPE_MIN_SCORE))) {
		free(ctx);
		return HB_TROPHY_ERROR_INVALID_TRP_LIST;
	}
	
	// Creating internal semaphores
	sprintf(tmp, "hbTrophy %s in_mutex", ctx->titleid);
	ctx->in_mutex = sceKernelCreateSema(tmp, 0, 0, 1, NULL);
	if (ctx->in_mutex < 0) {
		free(ctx);
		return HB_TROPHY_ERROR_TOO_MANY;
	}
	sprintf(tmp, "hbTrophy %s out_mutex", ctx->titleid);
	ctx->out_mutex = sceKernelCreateSema(tmp, 0, 0, 1, NULL);
	if (ctx->out_mutex < 0) {
		free(ctx);
		return HB_TROPHY_ERROR_TOO_MANY;
	}
	
	// Creating internal thread
	sprintf(tmp, "hbTrophy %s thread", ctx->titleid);
	ctx->thread = sceKernelCreateThread(tmp, &trophyThread, 0x10000100, 0x10000, 0, 0, NULL);
	if (ctx->thread < 0) {
		free(ctx);
		return HB_TROPHY_ERROR_TOO_MANY;
	}
	sceKernelStartThread(ctx->thread, sizeof(uint32_t), ctx);
	
	return HB_TROPHY_ERROR_OK;
}

int hbTrophyCreateHandle(hbTrophyHandle *handle) {
	// Checking for lib being initialized
	if (!lib_initialized) return HB_TROPHY_ERROR_NOT_INITIALIZED;
	
	*handle = 0xAAAAAAAA;
	return HB_TROPHY_ERROR_OK;
}

int hbTrophyDestroyHandle(hbTrophyHandle *handle) {
	// Checking for lib being initialized
	if (!lib_initialized) return HB_TROPHY_ERROR_NOT_INITIALIZED;
	if (*handle != 0xAAAAAAAA) return HB_TROPHY_ERROR_INVALID_HANDLE;
	
	*handle = 0xDEADBEEF;
	return HB_TROPHY_ERROR_OK;
}

int hbTrophyUnlockTrophy(hbTrophyContext context, hbTrophyHandle handle, hbTrophyId trophyId, hbTrophyId *platinumId) {
	// Checking for lib being initialized
	if (!lib_initialized) return HB_TROPHY_ERROR_NOT_INITIALIZED;
	
	
}
