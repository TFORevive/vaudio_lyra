# This program will convert the Lyra model files residing at lyra/model_coeffs
# into C++ header files, so that the models can be bundled within the binary itself
# instead of loading them from external .tflite files.

import argparse
import sys

def bin2header(data, var_name='var'):
    out = []
    out.append('unsigned char {var_name}[] = {{'.format(var_name=var_name))
    l = [ data[i:i+12] for i in range(0, len(data), 12) ]
    for i, x in enumerate(l):
        line = ', '.join([ '0x{val:02x}'.format(val=c) for c in x ])
        out.append('  {line}{end_comma}'.format(line=line, end_comma=',' if i<len(l)-1 else ''))
    out.append('};')
    out.append('unsigned int {var_name}_len = {data_len};'.format(var_name=var_name, data_len=len(data)))
    return '\n'.join(out)

out = "// AUTO-GENERATED\n\n"

files = {
    "lyra_config_proto": "lyra_config.binarypb",
    "lyragan": "lyragan.tflite",
    "quantizer": "quantizer.tflite",
    "soundstream_encoder": "soundstream_encoder.tflite"
}

for varname, filename in files.items():
    with open("lyra/model_coeffs/{filename}".format(filename=filename), 'rb') as f:
        out += bin2header(f.read(), varname) + "\n"

with open("lyra/model_coeffs/_models.h", 'w') as f:
    f.write(out)
