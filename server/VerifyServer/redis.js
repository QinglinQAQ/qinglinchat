const config_module = require('./config')
const Redis = require('ioredis')

// 创建Redis客户端
const RedisClient = new Redis({
  port: config_module.redis_port,           // Redis端口号
  host: config_module.redis_host,           // Redis主机地址
  password: config_module.redis_passwd,     // Redis密码
})


/*
    监听错误事件
*/
RedisClient.on('error', (err) => {
  console.error('Redis连接错误:', err);
  RedisClient.quit(); // 关闭Redis连接
});


async function GetRedis(key){
    try{
        const result = await RedisClient.get(key);
        if (result === null) {
            console.log(`Key "${key}" does not exist.`);
            return null
        }
        console.log(`Value for key "${key}": ${result}`);
        return result
    }catch(err){
        console.error('获取Redis值时出错:', err);
        return null
    }
}

// 查询 Redis是否存在某个 key
async function QueryRedis(key){
    try{
        const exists = await RedisClient.exists(key);
        if(exists == 0){
            console.log(`Key "${key}" does not exist.`);
            return null
        }
        console.log(`Key "${key}" exists.`);
        return exists
    }catch(error){
        console.error('查询Redis键时出错:', error);
        return null
    }
}

// 设置 Redis 键值对并设置过期时间（以秒为单位）
async function SetRedisExpire(key, value, expireTime){
    try{
        await RedisClient.set(key, value)
        await RedisClient.expire(key, expireTime)
        console.log(`Key "${key}" set with expiration of ${expireTime} seconds.`);
        return true;
    }catch(error){
        console.error('设置Redis键值时出错:', error);
        return false;
    } 
}

function QuitRedis(){
    RedisClient.quit(); // 关闭Redis连接
}

module.exports = {GetRedis,QueryRedis,SetRedisExpire,QuitRedis}