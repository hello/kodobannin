#!/usr/bin/python
from boto.s3.connection import S3Connection
from boto.s3.key import Key
import glob
import os
import sys
#TODO not hardcode this!
S3_ACCESS_ID = "AKIAJ7SHF3VSR7KB7VMA"
S3_SECRET_KEY = "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"
KODOBANNIN_BASE = "kodobannin/"
BUILD_BASE_KEY = KODOBANNIN_BASE + "builds/"
ALPHA_BASE_KEY = KODOBANNIN_BASE + "alpha/"
S3_ROOT = "hello-firmware"

conn = S3Connection(S3_ACCESS_ID, S3_SECRET_KEY)

def find_hexes(bin_dir):
    return glob.glob(os.path.join(bin_dir, "*.hex")) + glob.glob(os.path.join(bin_dir, "*.crc")) + glob.glob(os.path.join(bin_dir, "*.bin"))


def extract_name(hex_path):
    return os.path.basename(hex_path)

def extract_names(hex_paths):
    return [extract_name(h) for h in hex_paths]

def percent_cb(complete, total):
    sys.stdout.write('.')
    sys.stdout.flush()

def delete_folder(bucket, prefix_key):
    for key in bucket.list(prefix=prefix_key):
        print "Deleting Key: " + key.name
        key.delete()

def upload_files(flist, bucket, prefix):
    for f in flist:
        k = Key(bucket)
        k.key = os.path.join(prefix, extract_name(f))
        k.set_content_from_filename(f, cb = percent_cb, num_cb = 10)

def upload(commit_info = "limbo"):
    bucket = conn.lookup(S3_ROOT)
    if bucket:
        hexes = find_hexes("build")

        upload_files(hexes, bucket, os.path.join(BUILD_BASE_KEY, commit_info))
        if is_master():
            print "Building Alpha"
            delete_folder(ka, ALPHA_BASE_NAME)
            upload_files(hexes, bucket, os.path.join(ALPHA_BASE_NAME))

        print "Uploads finished"
        return True
    else:
        print "no such bukkit"
        return False

if len(sys.argv) > 1:
    upload(commit_info = sys.argv[1])
else:
    print "no commit, no upload"


