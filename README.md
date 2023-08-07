# vaudio_lyra [WIP!!!]

**Note:** This is purely untested pseudocode proof of concept currently. Do not expect it to work yet.

Speech codec plugin for voice that uses [Lyra](https://github.com/google/lyra), the new voice codec that achieves the best quality-to-bitrate ratio.

This was written for usage with TFORevive, and so may need adjustments for use with other Source Engine games.

## Compile (on Windows)

You have to have [Bazelisk installed](https://bazel.build/install/bazelisk), and also MSVC and Python (that has `numpy` and `six` installed with pip). Replace `C:\\Python311\\python.exe` below with your Python installation.

First run the following to convert model files into a header file that will be bundled into the DLL:
```
C:\\Python311\\python.exe .\models_to_header.py
```

Then to compile:

```
bazel build -c opt --action_env PYTHON_BIN_PATH="C:\\Python311\\python.exe" vaudio_lyra:vaudio_lyra
```

Final file is at `./bazel-bin/vaudio_lyra/vaudio_lyra.dll`

## Compiling CLI examples

Lyra comes with two sample CLI programs to test out encode/decode, you can compile and use them as follows:
```
bazel build -c opt --action_env PYTHON_BIN_PATH="C:\\Python311\\python.exe" lyra/cli_example:encoder_main

bazel-bin/lyra/cli_example/encoder_main.exe --input_path=lyra/testdata/sample1_16kHz.wav --output_dir=%temp% --bitrate=9200
bazel-bin/lyra/cli_example/decoder_main.exe --encoded_path=%temp%/sample1_16kHz.lyra --output_dir=%temp%/ --bitrate=9200
```

## Running unit tests

Running the built-in unit tests of Lyra might be useful, as we did some changes to statically build model files into the binaries themselves. So you can run them with:
```
bazel test --action_env PYTHON_BIN_PATH="C:\\Python311\\python.exe" //lyra:all
```

We don't currently have any vaudio_lyra-specific unit tests.
