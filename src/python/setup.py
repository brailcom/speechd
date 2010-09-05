#!/usr/bin/env python
from distutils.core import setup
setup(name='speechd',
      version='0.3',
      packages=['speechd'],
      )

setup(name='speechd_config',
      version='0.0',
      packages=['speechd_config'],
      scripts=['speechd_config/spd-conf']
      )
