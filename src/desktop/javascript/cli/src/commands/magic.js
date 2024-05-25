import chalk from 'chalk';
import register from '../requests/register.js';
import magic from '../requests/magic.js';

const magicDemo = async (passThrough) => {
  await register('green')
  .then((res) => magic(res.body, passThrough))
  .then((res) => {
    if(!res.magic) {
      throw(chalk.red('OH NO YOUR SHIT BLEW UP AGAIN!!!!'));
    }
    console.log(chalk.bgCyan(`Aww yeah! The resolver resolved your spell.`));
  })
  .catch(console.error);
};

export default magicDemo;
