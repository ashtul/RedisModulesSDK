#pragma once

#include <vector>
#include "redismodule.h"


namespace RedisModule {

///////////////////////////////////////////////////////////////////////////////////////////////

// this is under key digest API, maybe irrelevant as other digest fns are
// void * (*LoadDataTypeFromStringEncver)(const RedisModuleString *str, const RedisModuleType *mt, int encver);

///////////////////////////////////////////////////////////////////////////////////////////////

typedef ::RedisModuleCtx* Context;

///////////////////////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------------------------

class BlockedClient {
public:
	BlockedClient(Context ctx, RedisModuleCmdFunc reply_callback, RedisModuleCmdFunc timeout_callback,
		void (*free_privdata)(RedisModuleCtx *, void *), long long timeout_ms);
	int UnblockClient(void *privdata);
	int AbortBlock();

	operator RedisModuleBlockedClient *();
private:
	RedisModuleBlockedClient *_bc;
};

//---------------------------------------------------------------------------------------------

class ThreadSafeContext {
public:
	ThreadSafeContext(BlockedClient bc);
	ThreadSafeContext(Context ctx);
	~ThreadSafeContext();

	void Lock();
	int TryLock();
	void Unlock();
private:
	Context _ctx;
};

//---------------------------------------------------------------------------------------------

class RMType {
public:
	RMType(Context ctx, const char *name, int encver, RedisModuleTypeMethods *typemethods);
	RMType(RedisModuleType *type);

	operator RedisModuleType *() noexcept;
	operator const RedisModuleType *() const noexcept;

private:
	RedisModuleType *_type;
};

//---------------------------------------------------------------------------------------------

class String {
public:
    String(const char *ptr, size_t len);
    String(long long ll);
	String(unsigned long long ull);
    String(const RedisModuleString *str);

	String(const String& other) = delete;
	String(String&& other) = default;
	String& operator=(const String&) = delete;
	String& operator=(String&&) = delete;
    ~String();

	void Retain();

	const char *PtrLen(size_t &len) const noexcept;
    void AppendBuffer(const char *buf, size_t len);
	void Trim();

	int ToLongLong(long long& ll) const;
	int ToDouble(double& d) const;
    int ToLongDouble(long double& ld) const;
	int ULongLong(unsigned long long& ull) const;

	operator RedisModuleString *() noexcept;
	operator const RedisModuleString *() const noexcept;

private:
	RedisModuleString *_str;
};

int StringCompare(String& s1, String& s2) noexcept;

//---------------------------------------------------------------------------------------------

class Key {
public:
	Key(Context ctx, String& keyname, int mode);
	Key(RedisModuleKey *key);

	Key(const Key&) = delete;
	Key(Key&&) = delete;
	Key& operator=(const Key&) = delete;
	Key& operator=(Key&&) = delete;

	~Key() noexcept;
	int DeleteKey();
	
	size_t ValueLength() noexcept;
	
	mstime_t GetExpire() noexcept;
	int SetExpire(mstime_t expire);

	int Type() noexcept;
	RMType GetType() noexcept;

	void *GetValue() noexcept;
	int SetValue(RMType mt, void *value);

	operator RedisModuleKey *() noexcept;
	operator const RedisModuleKey *() const noexcept;

protected:
    RedisModuleKey *_key;
};

//---------------------------------------------------------------------------------------------

class StringKey : Key {
	StringKey(Context ctx, String& keyname, int mode);

	int Set(String& str);
	char *DMA(size_t &len, int mode); // direct memory access
	int Truncate(size_t newlen);
};

//---------------------------------------------------------------------------------------------

struct List : Key {
	List(Context ctx, String& keyname, int mode);

	int Push(int where, String& ele);
	String Pop(int where);

	String Get(long index);
	int Set(long index, String& value);
	int Insert(long index, String& value);
	int Delete(long index);
};

//---------------------------------------------------------------------------------------------

class Zset : Key {
public:
	Zset(Context ctx, String& keyname, int mode);

	int Add(double score, String& ele, int *flagsptr);
	int Incrby(double score, String& ele, int *flagsptr, double *newscore);
	int Rem(String& ele, int *deleted);
	int Score(String& ele, double *score);

	void RangeStop();
	int RangeEndReached();
	int FirstInScoreRange(double min, double max, int minex, int maxex);
	int LastInScoreRange(double min, double max, int minex, int maxex);
	int FirstInLexRange(String& min, String& max);
	int LastInLexRange(String& min, String& max);
	String RangeCurrentElement(double *score);
	int RangeNext();
	int RangePrev();
};

//---------------------------------------------------------------------------------------------

class Hash : Key {
public:
	Hash(Context ctx, String& keyname, int mode);

	template<typename... Vargs>
	int Set(int flags, Vargs... vargs);
	template<typename... Vargs>
	int Get(int flags, Vargs... vargs);
};

//---------------------------------------------------------------------------------------------

class IO {
public:
	Context GetContext();

	void SaveUnsigned(uint64_t value);
	uint64_t LoadUnsigned();

	void SaveSigned(int64_t value);
	int64_t LoadSigned();
	
	void SaveString(String& s);
	String LoadString();

	void SaveStringBuffer(const char *str, size_t len);
	char *LoadStringBuffer(size_t &len);
	
	void SaveDouble(double value);
	double LoadDouble();
	
	void SaveFloat(float value);
	float LoadFloat();
	
	template<typename... Vargs>
	void EmitAOF(const char *cmdname, const char *fmt, Vargs... vargs);

	template<typename... Vargs>
	void LogIOError(const char *levelstr, const char *fmt, Vargs... vargs);
private:
	RedisModuleIO *_io;
};

//---------------------------------------------------------------------------------------------

class CallReply {
public:
	template<typename... Vargs>
	CallReply(const char *cmdname, const char *fmt, Vargs... vargs);
	CallReply(RedisModuleCallReply *reply);
	~CallReply();
	const char *StringPtr(size_t &len);
	String CreateString();

	const char *Proto(size_t &len);
	int Type();
	long long Integer();
	size_t Length();
	double Double();
	int Bool();
	const char *BigNumber(size_t *len);
	const char *Verbatim(size_t *len, const char **format);
	CallReply SetElement(size_t idx);
	int MapElement(size_t idx, CallReply *key, CallReply *val);
	int AttributeElement(size_t idx, CallReply *key, CallReply *val);
	CallReply Attribute();

	CallReply ArrayElement(size_t idx);

	operator RedisModuleCallReply *();
private:
	RedisModuleCallReply *_reply;
};

//---------------------------------------------------------------------------------------------

class User {
public:
	User(const char *name);
	User(String& name);
	~User();

	int SetACL(const char* acl);
	int ACLCheckCommandPermissions(String *argv, int argc);
	int ACLCheckKeyPermissions(String& key, int flags);
	int ACLCheckChannelPermissions(String& ch, int literal);
	void ACLAddLogEntry(Context ctx, String& object, RedisModuleACLLogEntryReason reason);

	static String GetCurrentUserName(Context ctx);
	static int RedactClientCommandArgument(Context ctx, int pos);

private:
	RedisModuleUser *_user;
};

//---------------------------------------------------------------------------------------------

class Dict {
public:
	class Iter {
	public:
		Iter(RedisModuleDictIter *iter);
		~Iter();

		int ReseekC(const char *op, void *key, size_t keylen);
		int Reseek(const char *op, String& key);
		void *NextC(size_t *keylen, void **dataptr);
		void *PrevC(size_t *keylen, void **dataptr);
		String Next(void **dataptr);
		String Prev(void **dataptr);
		int CompareC(const char *op, void *key, size_t keylen);
		int Compare(const char *op, String& key);

		operator RedisModuleDict *();
	private:
		RedisModuleDictIter *_iter;
	};

	Dict();
	~Dict();
	
	uint64_t Size();
	
	int SetC(void *key, size_t keylen, void *ptr);
	int Set(String& key, void *ptr);
	int ReplaceC(void *key, size_t keylen, void *ptr);
	int Replace(String& key, void *ptr);
	void *GetC(void *key, size_t keylen, int *nokey);
	void *Get(String& key, int *nokey);
	int DelC(void *key, size_t keylen, void *oldval);
	int Del(String& key, void *oldval);
	Iter StartC(const char *op, void *key, size_t keylen);
	Iter Start(const char *op, String& key);
	
	operator RedisModuleDict *();

private:
	RedisModuleDict *_dict;
};

//---------------------------------------------------------------------------------------------

class Args {
public:
	Args(int argc, RedisModuleString **argv);

private:
	std::vector<String> _args;
};

//---------------------------------------------------------------------------------------------

template <class T>
struct CmdFunctor {
	CmdFunctor();

	virtual int Run(const Args& args);
	
	static int cmdfunc(Context ctx, Args& args);
};

//---------------------------------------------------------------------------------------------

namespace Alloc {
void *Alloc(size_t bytes);
void *TryAlloc(size_t bytes);
void *Calloc(size_t nmemb, size_t size);
void *Realloc(void *ptr, size_t bytes);
void Free(void *ptr);
char *Strdup(const char *str);
void *PoolAlloc(Context ctx, size_t bytes);

size_t Size(void *ptr);
size_t UsableSize(void *ptr);
size_t SizeString(String& str);
size_t SizeDict(Dict dict);
} // namespace Alloc

namespace Time {
mstime_t Milliseconds() noexcept;
uint64_t MonotonicMicroseconds() noexcept;
ustime_t Microseconds() noexcept;
ustime_t CachedMicroseconds() noexcept;
} // namespace Time

namespace EventLoop {
int Add(int fd, int mask, RedisModuleEventLoopFunc func, void *user_data);
int Del(int fd, int mask);
int AddOneShot(RedisModuleEventLoopOneShotFunc func, void *user_data);
} // namespace EventLoop

namespace Command {
int Create(Context ctx, const char *name, RedisModuleCmdFunc cmdfunc,
	const char *strflags, int firstkey, int lastkey, int keystep);
RedisModuleCommand *Get(Context ctx, const char *name);
int CreateSub(RedisModuleCommand *parent, const char *name, RedisModuleCmdFunc cmdfunc,
	const char *strflags, int firstkey, int lastkey, int keystep);
int SetInfo(RedisModuleCommand *command, const RedisModuleCommandInfo *info);

String FilterArgGet(RedisModuleCommandFilterCtx *fctx, int pos);
}

namespace Reply {
int WrongArity(Context ctx);
int Error(Context ctx, const char *err);
int Null(Context ctx);

int Array(Context ctx, long len);
void SetArrayLength(Context ctx, long len);

int Map(Context ctx, long len);
void SetMapLength(Context ctx, long len);

int Set(Context ctx, long len);
void SetSetLength(Context ctx, long len);

int Attribute(Context ctx, long len);
void SetAttributeLength(Context ctx, long len);

int Bool(Context ctx, int b);
int LongLong(Context ctx, long long ll);
int Double(Context ctx, double d);
int BigNumber(Context ctx, const char *bignum, size_t len);

int SimpleString(Context ctx, const char *msg);
int StringBuffer(Context ctx, const char *buf, size_t len);
int VerbatimStringType(Context ctx, const char *buf, size_t len, const char *ext);
int String(Context ctx, RedisModule::String& str);

int CallReply(Context ctx, RedisModule::CallReply reply);
}

namespace Info {
int AddSection(Context ctx, const char *name);
int BeginDictField(Context ctx, const char *name);
int AddFieldString(Context ctx, const char *field, String& value);
int AddFieldCString(Context ctx, const char *field, const char *value);
int AddFieldDouble(Context ctx, const char *field, double value);
int AddFieldLongLong(Context ctx, const char *field, long long value);
int AddFieldULongLong(Context ctx, const char *field, unsigned long long value);
}

namespace Config {
int RegisterBool(Context ctx, const char *name, int default_val, unsigned int flags,
	RedisModuleConfigGetBoolFunc getfn, RedisModuleConfigSetBoolFunc setfn,
	RedisModuleConfigApplyFunc applyfn, void *privdata);
int RegisterNumeric(Context ctx, const char *name, long long default_val, unsigned int flags,
	long long min, long long max, RedisModuleConfigGetNumericFunc getfn,
	RedisModuleConfigSetNumericFunc setfn, RedisModuleConfigApplyFunc applyfn, void *privdata);
int RegisterString(Context ctx, const char *name, const char *default_val, unsigned int flags,
	RedisModuleConfigGetStringFunc getfn, RedisModuleConfigSetStringFunc setfn,
	RedisModuleConfigApplyFunc applyfn, void *privdata);
int RegisterEnum(Context ctx, const char *name, int default_val, unsigned int flags,
	const char **enum_values, const int *int_values, int num_enum_vals,
	RedisModuleConfigGetEnumFunc getfn, RedisModuleConfigSetEnumFunc setfn,
	RedisModuleConfigApplyFunc applyfn, void *privdata);
int Load(Context ctx);
}

namespace DB_KEY { // TODO: better namespace
void AutoMemory(Context ctx) noexcept;

bool IsKeysPositionRequest(Context ctx) noexcept;
void KeyAtPos(Context ctx, int pos) noexcept;
void KeyAtPosWithFlags(Context ctx, int pos, int flags);
int KeyExists(Context ctx, String& keyname);

int *GetCommandKeysWithFlags(Context ctx, RedisModuleString **argv, int argc, int *num_keys, int **out_flags);

int GetSelectedDb(Context ctx) noexcept;
void SelectDb(Context ctx, int newid);

bool IsChannelsPositionRequest(Context ctx);
void ChannelAtPosWithFlags(Context ctx, int pos, int flags);

void Yield(Context ctx, int flags, const char *busy_reply);
int PublishMessageShard(Context ctx, String& channel, String& message);
int SendClusterMessage(Context ctx, const char *target_id, uint8_t type, const char *msg, uint32_t len);

template<typename... Vargs>
void Log(Context ctx, const char *level, const char *fmt, Vargs... vargs) noexcept;

template<typename... Vargs>
void Replicate(Context ctx, const char *cmdname, const char *fmt, Vargs... vargs);
void ReplicateVerbatim(Context ctx) noexcept;

int IsBlockedReplyRequest(Context ctx);
int IsBlockedTimeoutRequest(Context ctx);
void *GetBlockedClientPrivateData(Context ctx);

unsigned long long GetClientId(Context ctx) noexcept;
int SetClientNameById(uint64_t id, String& name);
String GetClientNameById(Context ctx, uint64_t id);
}

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace RedisModule
