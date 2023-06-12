#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <assert.h>

#include <android/log.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#define APPNAME "adbplay"

#define CASE2STR(v) case v: return #v
#define CASEOTHER(v) default: return ("(unknown" #v ")")

const char* SL_PLAYSTATE2STR(SLuint32 v) {
    switch (v) {
        CASE2STR(SL_PLAYSTATE_STOPPED);
        CASE2STR(SL_PLAYSTATE_PAUSED);
        CASE2STR(SL_PLAYSTATE_PLAYING);
        CASEOTHER(SL_PLAYSTATE);
    }
}

const char* SL_RESULT2STR(SLuint32 v) {
    switch (v) {
        CASE2STR(SL_RESULT_PRECONDITIONS_VIOLATED);
        CASE2STR(SL_RESULT_PARAMETER_INVALID);
        CASE2STR(SL_RESULT_MEMORY_FAILURE);
        CASE2STR(SL_RESULT_RESOURCE_ERROR);
        CASE2STR(SL_RESULT_RESOURCE_LOST);
        CASE2STR(SL_RESULT_IO_ERROR);
        CASE2STR(SL_RESULT_BUFFER_INSUFFICIENT);
        CASE2STR(SL_RESULT_CONTENT_CORRUPTED);
        CASE2STR(SL_RESULT_CONTENT_UNSUPPORTED);
        CASE2STR(SL_RESULT_CONTENT_NOT_FOUND);
        CASE2STR(SL_RESULT_PERMISSION_DENIED);
        CASE2STR(SL_RESULT_FEATURE_UNSUPPORTED);
        CASE2STR(SL_RESULT_INTERNAL_ERROR);
        CASE2STR(SL_RESULT_UNKNOWN_ERROR);
        CASE2STR(SL_RESULT_OPERATION_ABORTED);
        CASE2STR(SL_RESULT_CONTROL_LOST);
        CASEOTHER(SL_RESULT);
    }
}

SLObjectItf engineRoot = NULL;
SLEngineItf engine;
SLObjectItf outputMix = NULL;
SLObjectItf audioPlayer = NULL;
SLPlayItf playObj;

void tsprintf(const char* format, ...) {
    time_t rawtime;
    struct tm *timeinfo;
    char buffer[64];
    va_list args;

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(buffer, sizeof(buffer), "[%Y-%m-%d %H:%M:%S]", timeinfo);
    printf("%s", buffer);

    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    va_start(args, format);
    __android_log_vprint(ANDROID_LOG_VERBOSE, APPNAME, format, args);
    va_end(args);
}

void handle_SLresult(const char* op, SLresult result) {
    if (result != SL_RESULT_SUCCESS) {
        tsprintf("ERROR: Failure in %s(). Error code: %s\n", op, SL_RESULT2STR(result));
        exit(EXIT_FAILURE);
    }
    tsprintf("SUCCESS: %s()\n", op);
}

#define SL_CALLX(op,code) \
        (tsprintf("CALLING: %s()\n",(op)),handle_SLresult((op),(code)))

#define SL_CALLVOIDX(op,code) \
        (tsprintf("CALLING: %s()\n",(op)),(code),(handle_SLresult(op,SL_RESULT_SUCCESS)))

#define SL_CALL(obj,verb,args) \
        (SL_CALLX(#obj "->" #verb,(*(obj))->verb args))

#define SL_CALLVOID(obj,verb,args) \
        (SL_CALLVOIDX(#obj "->" #verb,(*(obj))->verb args))

#define SL_GETINF(obj,iid,out) \
        (tsprintf("CALLING: %s->GetInterface(%s)\n",(#obj),(#iid)),SL_CALL(obj,GetInterface,(obj, iid, &(out))))

#define SL_CLEANUP(obj) (SL_CALLVOID(obj,Destroy,(obj)))

void objectCallback(
	SLObjectItf caller,
	const void * pContext,
	SLuint32 event,
	SLresult result,
	SLuint32 param,
	void *pInterface) {
    ((void)caller);
    ((void)param);
    ((void)pInterface);
    tsprintf("OBJECT[%s]: event=%d result=%s\n", (char*)pContext,event,SL_RESULT2STR(result));
}

void enableObjectEvents(SLObjectItf object,char*what) {
    SL_CALL(object,RegisterCallback,(object, objectCallback, what));
}

void playCallback(SLPlayItf caller, void *context, SLuint32 event) {
    ((void)caller);
    ((void)context);
    tsprintf("PLAY: event=%d\n", event);
}

void enablePlayEvents(SLPlayItf play) {
    SL_CALL(play,RegisterCallback,(play, playCallback, NULL));
    SL_CALL(play,SetCallbackEventsMask,(play,
	SL_PLAYEVENT_HEADATEND|SL_PLAYEVENT_HEADATMARKER|SL_PLAYEVENT_HEADATNEWPOS|SL_PLAYEVENT_HEADMOVING|SL_PLAYEVENT_HEADSTALLED
    ));
}

void prefetchCallback(SLPrefetchStatusItf caller, void *context, SLuint32 event) {
    SLpermille fillLevel;
    SLuint32 prefetchStatus;

    ((void)context);
    SL_CALL(caller,GetFillLevel,(caller, &fillLevel));
    SL_CALL(caller,GetPrefetchStatus,(caller, &prefetchStatus));

    tsprintf("PREFETCH: fillLevel=%d, prefetchStatus=%u, event=%d\n", fillLevel, prefetchStatus, event);

    // Check for non-recoverable error
    if ((event & SL_PREFETCHEVENT_FILLLEVELCHANGE) && (event & SL_PREFETCHEVENT_STATUSCHANGE)) {
        if (fillLevel == 0 && prefetchStatus == SL_PREFETCHSTATUS_UNDERFLOW) {
            tsprintf("ABORTED: SL_PREFETCHSTATUS_UNDERFLOW\n");
	    exit(EXIT_FAILURE);
        }
    }
}

void enablePrefetchEvents(SLObjectItf playerObject) {
    SLPrefetchStatusItf prefetchStatusItf;
    SL_GETINF(playerObject,SL_IID_PREFETCHSTATUS,prefetchStatusItf);
    SL_CALL(prefetchStatusItf,RegisterCallback,(prefetchStatusItf, prefetchCallback, NULL));
    SL_CALL(prefetchStatusItf,SetCallbackEventsMask,(prefetchStatusItf, SL_PREFETCHEVENT_FILLLEVELCHANGE | SL_PREFETCHEVENT_STATUSCHANGE));
}

#define EVAL(...) __VA_ARGS__
#define DCL_IFREQ(name,interfaces,reqs) \
	const SLInterfaceID name##$IDS[] = EVAL(EVAL interfaces); \
	const SLboolean name##$REQ[] = EVAL(EVAL reqs); \
	const size_t name##$COUNT = ((sizeof(name##$IDS)/sizeof(SLInterfaceID))); \
	static_assert (((sizeof(name##$IDS)/sizeof(SLInterfaceID))) == ((sizeof(name##$REQ)/sizeof(SLboolean))), "IDS and REQ count mismatch")

#define USE_IFREQ(name) \
	(name##$COUNT),(name##$IDS),(name##$REQ)

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <path_to_audio_file>\n", argv[0]);
        return 1;
    }

    const char *path = argv[1];
    tsprintf("Playing audio file: %s\n", path);

    // Create the engine object
    const SLEngineOption engineOptions[] = { {(SLuint32) SL_ENGINEOPTION_THREADSAFE, (SLuint32) SL_BOOLEAN_TRUE} };
    DCL_IFREQ(engineIfReq,({SL_IID_SEEK, SL_IID_MUTESOLO }),({SL_BOOLEAN_FALSE, SL_BOOLEAN_FALSE}));
    handle_SLresult("slCreateEngine",slCreateEngine(&engineRoot, 1, engineOptions, USE_IFREQ(engineIfReq)));
    SL_CALL(engineRoot,Realize,(engineRoot, SL_BOOLEAN_FALSE));
    SL_GETINF(engineRoot,SL_IID_ENGINE,engine);

    // Create the output mix object
    DCL_IFREQ(outputMixIfReq,({}),({}));
    SL_CALL(engine,CreateOutputMix,(engine, &outputMix, USE_IFREQ(outputMixIfReq)));
    SL_CALL(outputMix,Realize,(outputMix, SL_BOOLEAN_FALSE));

    // Configure the audio source
    SLDataLocator_URI uriLocator = {SL_DATALOCATOR_URI, (SLchar *) path};
    SLDataFormat_MIME formatMIME = {SL_DATAFORMAT_MIME, NULL, SL_CONTAINERTYPE_UNSPECIFIED};
    SLDataSource audioSource = {&uriLocator, &formatMIME};

    // Configure the audio sink
    SLDataLocator_OutputMix locatorOutputMix = {SL_DATALOCATOR_OUTPUTMIX, outputMix};
    SLDataSink audioSink = {&locatorOutputMix, NULL};

    // Create the audio player
    DCL_IFREQ(playerIfReq,({SL_IID_SEEK, SL_IID_MUTESOLO, SL_IID_PREFETCHSTATUS, SL_IID_PLAY}),({SL_BOOLEAN_FALSE, SL_BOOLEAN_FALSE, SL_BOOLEAN_TRUE , SL_BOOLEAN_TRUE }));
    SL_CALL(engine,CreateAudioPlayer,(engine, &audioPlayer, &audioSource, &audioSink, USE_IFREQ(playerIfReq)));
    enableObjectEvents(audioPlayer,"audioPlayer");
    SL_CALL(audioPlayer,Realize,(audioPlayer, SL_BOOLEAN_FALSE));
    enablePrefetchEvents(audioPlayer);
    SL_GETINF(audioPlayer,SL_IID_PLAY,playObj);
    enablePlayEvents(playObj);

    // Start playing the audio
    SLuint32 state;
    SL_CALL(playObj,GetPlayState,(playObj, &state));
    tsprintf("[Before SetPlayState] PlayState=%s\n",SL_PLAYSTATE2STR(state));
    SL_CALL(playObj,SetPlayState,(playObj, SL_PLAYSTATE_PLAYING));
    SL_CALL(playObj,GetPlayState,(playObj, &state));
    tsprintf("[After SetPlayState] PlayState=%s\n",SL_PLAYSTATE2STR(state));

    // Wait for audio to finish playing
    while (state != SL_PLAYSTATE_STOPPED) {
        SL_CALL(playObj,GetPlayState,(playObj, &state));
        tsprintf("PlayState=%s\n",SL_PLAYSTATE2STR(state));
        sleep(1);
    }

    // Clean up
    SL_CLEANUP(audioPlayer);
    SL_CLEANUP(outputMix);
    SL_CLEANUP(engineRoot);

    return 0;
}
