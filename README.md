# Adversarial Examples of Go AIs
-----

This repository contains demonstrations of adversarial examples in Go AIs and code for conducting our proposed adversarial attack. Our work has been published at NeurIPS 2022:

**"Are AlphaZero-like Agents Robust to adversarial Perturbations?"**  
NeurIPS 2022  
Li-Cheng Lan, Huan Zhang, Ti-Rong Wu, Meng-Yu Tsai, I-Chen Wu, and Cho-Jui Hsieh. 

A Chinese version of this documentation can be found at [中文版](https://github.com/lan-lc/adversarial_example_of_Go/blob/main/chineseREADME.md).

## A Quest for Adversarial Attacks of Go AIs

In our paper, we attack famous AlphaZero-based Go agents like [KataGo](https://github.com/lightvector/KataGo), [LeelaZero](https://github.com/leela-zero/leela-zero), [ELF](https://github.com/pytorch/ELF), and [CGI](https://arxiv.org/abs/2003.06212) to see **if well-trained Go agents will make trivial mistakes that even amateur players can easily tell?** Although researchers often find that AIs can be fooled by crafting malicious "adversarial examples" that are very similar to natural inputs, can we find such adversarial examples for super-human Go AIs?

### Definition of adversarial example in Go

In image and NLP domains, adversarial examples are visually or semantically similar to an benign input. In a discrete-state game like Go, we first extend the definition of an adversarial example to the state (defined by stones on the game board) in the game of Go. An adversarial state is "semnatically similar" to a natural state (e.g., in a known game record) and also fools the Go AI; more precisely:

1. The adversarial state should be *at most two stones* away from a natural state;
2. The adversarial state should be semantically equivalent to the natural state (have the same turn color, winrate, and the best move); in other words, the added one or two stones are *meaningless* for the game.
3. The target Go agent is correct at the natural state but *obviously wrong* on the adversarial state.

Due to the complexity of Go, to produce convincing adversarial examples, we additional require that even amateur human players can verify that the natural state and the adversarial state are semantically equivalent (point 2) and the target agent makes a serious mistake (point 3).

In the following two paragrams, we show two of the adversarial examples we found that satisfy the definition above with detailed explanations. More examples are shown in [Adversarial Examples](#adversarial-examples) section.

### Attacking Go agent's policy

![](./images/f12.png)

Fig. 2 shows an adversarial example that makes KataGo (with 50 MCTS simulations) output a wrong move. 
First, the state in Fig. 2 is created by adding $\color{#9933FF} \text{two stones}$ (marked as $\color{#9933FF} \text{1}$ and $\color{#9933FF} \text{2}$) to a natural state (Fig. 1) of AlphaGo Zero self-play record. 
Even amateur players can tell that Fig.1 and Fig.2 are semantically equivalent since it is obvious that both of the additional $\color{#9933FF} \text{stones}$ are meaningless and will not affect the winrate nor the best action of the state in Fig 1. 
The best action of both states is playing black at position $\color{green} \text{E1 ◆}$ since by playing $\color{green} \text{E1 ◆}$ black can kill all the white stones that are marked with blue triangles.
However, in Fig.2, the KataGo agent will play black at position $\color{red} \text{E11 ◆}$ instead of position $\color{green} \text{E1 ◆}$ even after executing 50 MCTS simulations. Although playing black at $\color{red} \text{E11 ◆}$ can save the four black stones marked with squares,  even amateur human players can tell that killing the white stones marked with triangles is a more important move. In fact, after playing the bad move at $\color{red} \text{E11 ◆}$, the winrate estimated by KataGo drops in the next round.


To understand why KataGo makes this mistake,  we list the node information of the MCTS first layer on the right side of Fig. 2.
The first column shows the node's action. The second column is the number of MCTS simulations of each node. The third column is the predicted winrate of each node. According to the list, we can see that KataGo did consider the position $\color{green} \text{E1 ◆}$ once. However, due to the adversarial perturbation added, the predicted winrate of that move is too low, KataGo stops exploring the node $\color{green} \text{E1 ◆}$ and missed the opportunity of finding the best move.

### Attacking Go agent's value function (winrate)

![](./images/f34.png)

Fig. 4 shows an adversarial example that will make KataGo with 50 simulation output a wrong winrate for this step. Fig. 4 is created by adding a $\color{#9933FF} \text{white stone (1)}$ to the natural state in Fig. 3. On Fig. 4 the white player got an extra (but most certainly dead) white stone. On both figures, it is the white player's turn to play. Although the additional  $\color{#9933FF} \text{white stone (1)}$ is obviously meaningless even for amateur human players (at least, this extra white stone should not be harmful for white), KataGo outputs a *much lower* winrate for white, which is unexpected. 

### Algorithm details

To find the adversarial examples that satisfy our definition, like Fig. 2 and Fig.4,
we carefully designed the constraints on perturbed states during a search procedure so that they are semantically similar to the original states and are also easy enough for human players to verify the correct move. Next, we test AZ agents with thousands of these perturbed
states to see if they will make trivial mistakes. We also design an efficient algorithm to make the testing faster. Usually, our method is 100 times faster than brute force search. For more details, please read [our paper](https://arxiv.org/).

### How often can we find adversarial examples for a super-human AI?

The following table shows the results of attacking KataGo with AlphaGo Zero self-play games, and we increase the power of the Go Agent by increase the number of MCTS simulations.

![](./images/table.png)

The first column shows the number of MCTS simulations used by KataGo. The second and the third columns show the success rate of making KataGo output a bad action (policy attack). The third and fourth columns show the success rate of making KataGo output an extremely wrong winrate (value attack). We can see that as the number of MCTS simulations increases, KataGo becomes harder to attack. However, even with 50 MCTS simulations (which is already super-human), we can still find policy adversarial examples in 68% of the AlphaGo Zero self-play games. 

## Code for attacking Go AIs

We now provide instructions for finding adversarial examples in Go agents, like those demonstrated above.
Our codebase also aims to be a useful and lightweight analysis tool for Go players and developers and support the following features:

- Able to connect different agents on different machines through ports.
- Able to conduct our attack on the Go program that supports GTP (https://senseis.xmp.net/?GoTextProtocol). 
- Able to save and load the MCTS and NN results of a program.
- Able to let two agents play against each other.
- Able to store the analysis results in an SGF file.

### Setup


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

## Adversarial Examples
The following pictures show some of the adversarial examples we found. For each pair of pictures, the left is the natural state, and the right is the adversarial state. More examples are shown in the [examples](https://github.com/lan-lc/adversarial_example_of_Go/tree/main/examples) folder.

![](./images/f56.png)
![](./images/f78.png)
![](./images/f910.png)
![](./images/f1112.png)
![](./images/f1314.png)
![](./images/f1516.png)

<!-- 
<img src="./images/f56.png" height="400"/>
<img src="./images/f78.png" height="400"/>
<img src="./images/f710.png" height="400"/>
<img src="./images/f1112.png" height="400"/>
<img src="./images/f1314.png" height="400"/>
<img src="./images/f1516.png" height="400"/> -->
