
const grpc = require('@grpc/grpc-js')
const message_proto = require('./proto')
const const_module = require('./const')
const {v4 : uuidv4}  = require('uuid')
const emailModule = require('./email')
const redisModule = require('./redis')

async function GetVerifyCode(call, callback) {
    console.log("[", new Date().toLocaleString(), "]:", "email is ", call.request.email)
    try{
        let query_res = await redisModule.GetRedis(const_module.code_prefix + call.request.email)
        console.log("query redis result is ", query_res)
        let uniqueId = query_res
        if(query_res == null){
            // 生成唯一标识符
            uniqueId = uuidv4()
            if(uniqueId.length > 4){
                uniqueId = uniqueId.substring(0, 4)
            }
            let bres = await redisModule.SetRedisExpire(const_module.code_prefix + call.request.email, uniqueId, 600)
            if(!bres){
                console.log("set redis failed")
                callback(null, { email:  call.request.email,
                    error:const_module.Errors.RedisErr
                }); 
                return;
            }

        }
        
        console.log("uniqueId is ", uniqueId)
        let text_str =  '您的验证码为'+ uniqueId +'请三分钟内完成注册'
        //发送邮件
        let mailOptions = {
            from: 'm15387861404@163.com',
            to: call.request.email,
            subject: '验证码',
            text: text_str,
        };
    
        let send_res = await emailModule.SendMail(mailOptions);
        console.log("send res is ", send_res)

        callback(null, { email:  call.request.email,
            error:const_module.Errors.Success
        }); 
        
 
    }catch(error){
        console.log("catch error is ", error)
        callback(null, { email:  call.request.email,
            error:const_module.Errors.Exception
        }); 
    }
     
}

function main() {
    var server = new grpc.Server()
    server.addService(message_proto.VerifyService.service, { GetVerifyCode: GetVerifyCode })
    server.bindAsync('0.0.0.0:50051', grpc.ServerCredentials.createInsecure(), () => {
        server.start()
        console.log('grpc server started')        
    })
}

main()