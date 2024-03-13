Kobra_1.0.nnue was created from the many millions of self-play games created during development and testing.

The 'convert_bin' & 'learn' functions from nodchip's NNUE repo were used.

See: https://github.com/nodchip/Stockfish/releases/tag/stockfish-nnue-2020-08-30

The 'convert_bin' function converts .pgn(text) files to binary format.

for ex: "convert_bin output_file_name kobra_1.0.bin kobra_1.0.txt"

The 'learn' function loads the resulting binary data to train and create the NNUE net.

for ex: "learn targetdir traindata loop 100 batchsize 1000000 eta 1.0 lambda 0.5 eval_limit 32000 nn_batch_size 1000 newbob_decay
0.5 eval_save_interval 10000000 loss_output_interval 1000000 mirror_percentage 50 validation_set_file_name valdata\kobra-val.bin"
