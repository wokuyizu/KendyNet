#include "asynredis.h"
#include "systime.h"
#include "log.h"

static void request_destroyer(void *ptr)
{
	free_dbrequest((db_request_t)ptr);	
}

void dorequest(struct asynredis *redis,db_request_t req)
{
	while(redis->context){
		redisReply *r = redisCommand(redis->context,to_cstr(req->query_str));
		if(r){
			if(req->callback){
				db_result_t result = new_dbresult(db_redis,r,req->callback,req->ud);
				asyndb_sendresult(req->sender,result);
			}			
			break;
		}else
		{
			redisFree(redis->context);
			uint32_t trytime = 0;
			while((redis->context = redisConnect(redis->ip,redis->port)) == NULL){
				if(++trytime >= 64){
					SYS_LOG(LOG_ERROR,"to redis %s:%d error\n",redis->ip,redis->port);
					break;
				}
				sleepms(50);
			}
		}	
	}
	request_destroyer((void*)req);
}


static void *worker_main(void *ud){

	struct asynredis *redis = (struct asynredis*)ud;
	while(redis->stop == 0){
		db_request_t req = NULL;
        msgque_get(redis->mq,(lnode**)&req,10);
        if(req)
        	dorequest(redis,req);
	}
    return NULL;
}

int32_t redis_asyn_request(asyndb_t asyndb,db_request_t req)
{
	if(!asyndb) return -1;
	struct asynredis *redis = (struct asynredis*)asyndb;
	if(0 != msgque_put_immeda(redis->mq,(lnode*)req))
	{	
		free_dbrequest(req);
		printf("push request error\n");	
		return -1;
	}
	return 0;
}

db_result_t redis_sync_request(struct asyndb *asyndb,const char *req)
{
	struct asynredis *redis = (struct asynredis*)asyndb;
	if(!redis || !redis->context)
		return NULL;
	redisReply *r = redisCommand(redis->context,req);
	if(!r){
		redisFree(redis->context);
		uint32_t trytime = 0;
		while((redis->context = redisConnect(redis->ip,redis->port)) == NULL){
			if(++trytime >= 64){
				SYS_LOG(LOG_ERROR,"to redis %s:%d error\n",redis->ip,redis->port);
				return NULL;
			}
			sleepms(50);
		}
	}
	return new_dbresult(db_redis,r,NULL,NULL);		
}

void    redis_destroy(asyndb_t asyndb)
{
	struct asynredis *redis = (struct asynredis*)asyndb;
	redis->stop = 1;
	thread_join(redis->worker);
	redisFree(redis->context);
	destroy_thread(&redis->worker);
	free(asyndb);
}

asyndb_t redis_new(const char *ip,int32_t port)
{	
	redisContext *c = redisConnect(ip,port);
	if(c->err){
		redisFree(c);
		return NULL;
	}
	struct asynredis *redis = calloc(1,sizeof(*redis));
	redis->mq =  new_msgque(32,request_destroyer);
	redis->base.asyn_request = redis_asyn_request;
	redis->base.sync_request = redis_sync_request;
	redis->base.destroy_function = redis_destroy;	
	redis->context = c;
	redis->worker = create_thread(THREAD_JOINABLE);
	redis->stop = 0;
	strcpy(redis->ip,ip);
	redis->port = port;
	thread_start_run(redis->worker,worker_main,(void*)redis);	
	return (asyndb_t)redis;
}
