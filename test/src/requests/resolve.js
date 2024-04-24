import sessionless from 'sessionless-node';
import superagent from 'superagent';
import config from '../../config/local.js';

const resolve = async (bodyFromRegistration) => {
  const { uuid, color } = bodyFromRegistration;

  const colorURL = config.colors[color].serverURL;
  const colorSignaturePlacement = config.colors[color].signature;

  let message = {
    uuid,
    coolness: 'max',
    timestamp: new Date().getTime() + ''
  };

console.log(message);

  let signature = await sessionless.sign(JSON.stringify(message));

  let post = superagent.post(colorURL + '/resolve');

  if(colorSignaturePlacement === 'payload') {
    const payload = {
      ...message,
      signature
    };
    post = post.send(payload)
  } else {
    post = post.send(message)
               .set('signature', signature);
  }
  return post.set('Content-Type', 'application/json')
             .set('Accept', 'application/json')
         .then(res => {
           res.body.color = color;
           return res.body;
         });
};

export default resolve;
