# Chromium 49.0.2623.112

This directory contains chromium project source code of version 49.0.2623.112.

It's mainly a copy of https://github.com/chromium/chromium/tree/49.0.2623.112


# What changes made on.

I put a missing third_party project in this repo:

`opus@cae696156f1e60006e39821e79a1811ae1933c69`

Which is defined in DEPS but the git link is broken.

For the origin source of `opus@cae69615`,
see here: [android.googlesource.com](https://android.googlesource.com/platform/external/chromium_org/third_party/opus/src/+/3add326b8269bc061065676d63a610951c1329f0)

# Steps to build Chromium 49

1、Make a directory for doing this.
`mkdir chrome49` 

2、Add a .gclient file, which defined a url targeting to this repo.

chrome49/.gclient
```json
solutions = [
  {
    "managed": False,
    "name": "src",
    "url": "https://github.com/linsmod/linsknife.git",
    "custom_deps": {},
    "deps_file": "DEPS",
    "safesync_url": "",
  },
]
```

3、Ensure Python3 installed. I'm using 3.10.12,
We still have to install version 2.7, we'll use it below.


4、Get depot_tools if you do not have one.
`git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git` 


Switch your depot_tools brach to chrome/4147：
`git branch chrome/4147`
You may need clean uncommit files after switching brach.

Add to PATH
`export PATH=~/depot_tools:$PATH`

otherwise you will got gn error 
`Unknown function: set_sources_assignment_filter`



5、Obtain all code.

 `cd chrome49/` 
 `gclient sync`
This command also download third_party dependencies defined in DEPS after this repo is pull into `chrome49/src` successful.
Downloading third_party dependencies will take much time.

if `gclient sync` succeed, the sub command `gclient runhooks` also succeed.


It means the prerequisite binaries to build chrome49 is downloaded and prepared, including the `gn` tool.

If `gclient runhooks` error, try switch to python2 and retry `gclient runhooks`

6、Generate ninja files and start building:
`gn gen out/Default`
`ninja -C out/Default chrome`

# Errors you may meet
### fatal error: 'XPathGrammar.hpp' file not found
Check the bison version installed on you system, 

I solved this by downgrading bison version from `3.7.2` to `3.6.4`.

Download `bison:3.6.4` from here: `https://ftp.gnu.org/gnu/bison/bison-3.6.4.tar.gz`

Unpack it into some where then make install it.
`./configure && make && sudo make install`

