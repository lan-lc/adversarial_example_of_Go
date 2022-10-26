# Adversarial_Example_of_Go
------
## Overview
This repository contains a reference implementation of finding adversarial examples for Go Agents.

Given a game, our method can efficiently find an adversarial example of a target agent. The adversarial example is only two stones away from one of the state in the game. It can make the target agent output a extremely wrong action or value. The following picture shows on of the example we found. 
![Screenshot](game11)
In this state, KataGo agent, one of the best AZ agents, will want to play black at position B instead of postion A even after executing 50 MCTS simulation. Even amateur human players can tell that position A is the best action since black can kill the white stones that are marked with blue triangle. Moreover, the this state is created by only adding two stones to an AlphaGo Zero self-play record, which are marked as 1 and 2. With the examples we found, we show that AZ agents has not yet generalized enough to be better than amateur human players on all the states.

Our code can conduct attack on four public agents: KataGo, Leela, ELF, and CGI. Our code can easily attack any other Go agents that support GTP (https://senseis.xmp.net/?GoTextProtocol) by adding an additional derived class. Since we connect the agent with local port, each agent can run on its own docker as long as its docker installed (https://github.com/Remi-Coulom/gogui). To our best knowledge, our code is the most light weight program that can interact with the four public agents. 

Our code also extend to other discrete games. For the rules of Go, instead of writing our own, we use gnugo (https://www.gnu.org/software/gnugo/) to judge which action is legal. Therefore, to apply our method to another game, we just need another agent to provide the rules.

To speed up the search, our will also be used to 



## Setup


```bash
git clone git@github.com:lan-lc/adversarial_example_of_Go.git
cd adversarial_example_of_Go
sudo docker run --gpus all --network="host" --ipc=host -it -v=$PWD:/workspace kds285/go-attack
mkdir build
cd build
cmake ..
make -j
```
### KataGo
```bash
# run container
sudo docker run --gpus all --network="host" --ipc=host -it kds285/katago
# run program with gogui-server
gogui-server -port 9999 -loop "./katago gtp -model kata1-b40c256-s9948109056-d2425397051.bin.gz -config gtp_example.cfg"
# run program (20 blocks) with gogui-server
gogui-server -port 9999 -loop "./katago gtp -model kata1-b20c256x2-s5303129600-d1228401921.bin.gz -config gtp_example.cfg"
```
### Leela
```bash
# run container
sudo docker run --network="host" --ipc=host -it kds285/leelazero:go_attack
# run program with gogui-server
echo "./leelaz --gtp --threads 1 --noponder --visits 800 --resignpct 0 --timemanage off --gpu \$1 2>/dev/null" > run_gtp.sh
chmod +x run_gtp.sh
gogui-server -port 9998 -loop "./run_gtp.sh 0"
```
### ELF
```bash
# run container
sudo docker run --network="host" --ipc=host -it kds285/elf-opengo:go_attack
# run program with gogui-server
echo "./gtp.sh /go-elf/ELF/pretrained-go-19x19-v2.bin --verbose --gpu \$1 --num_block 20 --dim 256 --mcts_puct 1.50 --batchsize 8 --mcts_rollout_per_batch 8 --mcts_threads 2 --mcts_rollout_per_thread 400 --resign_thres 0 --mcts_virtual_loss 1 2>&1 | grep --line-buffered \"^= \|custom_output\" | awk '{ if(\$1==\"[custom_output]\") { print \$0; } else { print \$0\"\n\"; system(\"\"); } }'" > run_gtp.sh
chmod +x run_gtp.sh
gogui-server -port 9997 -loop "./run_gtp.sh 0"
```
### CGI
```bash
sudo docker --network="host" --ipc=host -it kds285/cgigo:go_attack
# build program
./scripts/setup-cmake.sh release caffe2 && make -j
# run program with gogui-server
gogui-server -port 9999 "Release/CGI -conf_file cgi_example.cfg"
```
### Gnugo

