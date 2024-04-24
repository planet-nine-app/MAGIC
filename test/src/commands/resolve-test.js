import chalk from 'chalk';
import register from '../requests/register.js';
import resolve from '../requests/resolve.js';

const resolveTest = async (color, language) => {
  await register(color)
  .then((res) => doCoolStuff(res.body))
  .then((res) => {
    if(!res.doubleCool) {
      throw(chalk.red('OH NO YOUR SHIT BLEW UP AGAIN!!!!'));
    }
    console.log(chalk[color](`Aww yeah! The ${language ? language : ''} server thinks you're double cool.`));
  })
  .catch(console.error);
};

export default resolveTest;
