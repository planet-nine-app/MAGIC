let fountUser;
let allyabaseUser;

async function post(url, payload) {
  return await fetch(url, {
    method: 'post',
    body: JSON.stringify(payload),
    headers: {'Content-Type': 'application/json'}
  });
};

function getPage($item) {
  return $item.parents('.page').data('data');
};

function getAllyabaseUser(item) {
  if(item.allyabaseUser) {
    return item.allyabaseUser;
  } else {
    return fetch('/plugin/ftt/user').then(res => res.json());
  }
};

function getBDOs($item, page) {
  let bdoPromises = [];
  if(page.transferees) {
    page.transferees.forEach(transferee => {
      if(!transferee.bdoPubKey) {
        return;
      }
      const transfereeDiv = document.createElement('div');
      transfereeDiv.innerHTML = '<p>Fetching transfer details for ${transferee.bdoUUID}</p>';
      $item.append(transfereeDiv);
      const bdoPromise = fetch(`/plugin/ftt/bdo?pubKey=${transferee.bdoPubKey}`)
        .then(bdo => {
          if(bdo.bdoUUID) {
            transfereeDiv.innerHTML = `<p>Transferee: ${bdo.bdoUUID} at ${bdo.host}</p>
              <button id="${transferee.bdoPubKey}">Advance</button>`;
          } else {
            transfereeDiv.innerHTML = '<p>No BDO uuid found for this transferee</p>';
          }
        });
      bdoPromises.push(bdoPromise);
    });

    return Promise.all(bdoPromises);
  } else {
    return $item.append(`<div><p>You have no transferees yet. Get on that marketing!</p></div>`);
  }
};

function getSignedFount(allyabaseUser, $item, item) { 
  if(item.signature) {
    const storyText = getPage($item).story.map($ => $.text).join('');
    const message = item.timestamp + item.host + allyabaseUser.fountUser.uuid + allyabaseUser.bdoUser.uuid + storyText;
    return fetch(`/plugin/ftt/verify?signature=${item.signature}&message=${message}`)
      .then(verified => {
        if(verified) {
          $item.append(`<div><p>This content is signed and verified!</p></div>`);
        } else {
          $item.append(`<div><p>This content is not signed yet. Hit the Sign button to sign it.</p></div>`);
        }
      })
      .catch(err => console.warn('got an error with signature'));
  }
};

function getTransferees($item, item, allyabaseUser) {
console.log('what is allyabaseUser here with transferees', allyabaseUser);
  if(!allyabaseUser) {
    $item.append(`<div><p>No Allyabase User with transferees</p></div>`);
    return;
  }
console.log('about to do transferPromises');
  let transfereesPromises = [];
  const transferees = allyabaseUser.bdoUser && allyabaseUser.bdoUser.bdo && allyabaseUser.bdoUser.bdo.transferees;
  Object.keys(transferees).forEach(uuid => {
console.log('transferees', transferees);
console.log('uuid', uuid);
console.log('transferees[uuid]', transferees[uuid]);
    const path = decodeURIComponent(transferees[uuid]);
console.log('getting user from ', path);
    const prom = fetch(path)
      .then(resp => resp.json())
      .then(transferee => {
console.log('transferee looks like: ', transferee);
        let sodoto = '';
        if(!transferee.fountUser) {
          transferee.fountUser = {};
        }
        switch(transferee.fountUser.nineumCount) {
          case 0: sodoto = 'signed up';
            break;
          case 1: sodoto = 'seen one';
            break;
          case 2: sodoto = 'seen one and done one';
            break;
          case 3: sodoto = 'seen one and done one and taught one (sodoto)';
            break;
          default: sodoto = 'seen one and done one and taught one (sodoto)';
            break;
        } 
        $item.append(`<div><p>The transferee at ${transferees[uuid]} has ${sodoto}.</p></div>`);
        $item.append(`<div><p>Grant the next level token to transferee at ${transferees[uuid]}?   <button id=${uuid}>Advance</button></p></div>`);
      });
    transfereesPromises.push(prom);
  });
  return Promise.all(transfereesPromises);
};

function emit($item, item) {
  $item.empty(item);

  const gettingUserDiv = document.createElement('div');
  gettingUserDiv.innerHTML = '<p>Getting your allyabase user, and signatures...</p>';
  $item.append(gettingUserDiv);
  let user;

  getAllyabaseUser(item)
    .then(_allyabaseUser => {
      allyabaseUser = _allyabaseUser;
      user = allyabaseUser;
console.log('allyabaseUser', allyabaseUser);
      item.allyabaseUser = allyabaseUser;
      gettingUserDiv.remove();
      gettingUserDiv.innerHTML = `<p>Welcome back ${allyabaseUser.fountUser.uuid} you have ${allyabaseUser.nineum.length} nineum</p>`;
      $item.append(gettingUserDiv);
      return getSignedFount(allyabaseUser, $item, item);
    })
//    .then(getBDOs($item, page))
    .then(() => {
      return getTransferees($item, item, user);
    })
    .catch(err => console.warn('received an error emitting in ftt plugin', err))
    .finally(() => {
      setTimeout(() => {
	const transferees = allyabaseUser.bdoUser && allyabaseUser.bdoUser.bdo && allyabaseUser.bdoUser.bdo.transferees;
	Object.keys(transferees).forEach(uuid => {
      console.log('adding the button to', uuid);
	  $item.find(`#${uuid}`).click(() => {
      console.log('the grant button has been clicked');
	    post('/plugin/ftt/grant-nineum', {toUUID: uuid, flavor: '24071209a3b3'})
	      .then(transferee => {
		if(transferee) {
		  $item.append('<div><p>Transfer Successful!</p></div>');
		} else {
		  $item.append('<div><p>Transfer Unsuccessful!</p></div>');
		}
	      });
	  });
	});
      }, 5000)
    });
};

function bind($item, item) {
  if(!allyabaseUser) {
    return;
  }
console.log('trying to add the button');
  const transferees = allyabaseUser.bdoUser && allyabaseUser.bdoUser.bdo && allyabaseUser.bdoUser.bdo.transferees;
  Object.keys(transferees).forEach(uuid => {
console.log('adding the button to', uuid);
    $item.find(`#${uuid}`).click(() => {
console.log('the grant button has been clicked');
      post('/plugin/ftt/grant-nineum', {toUUID: uuid, flavor: '24071209a3b3'})
        .then(transferee => {
          if(transferee) {
            $item.append('<div><p>Transfer Successful!</p></div>');
          } else {
            $item.append('<div><p>Transfer Unsuccessful!</p></div>');
          }
        });
    });
  });
};

if(window) {
  window.plugins['ftt'] = {emit, bind};
}

export const ftt = typeof window == 'undefined' ? { emit, bind } : undefined;
