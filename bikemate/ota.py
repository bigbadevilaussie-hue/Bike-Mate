# === Bike-Mate GUI: ota ===
# auto-extracted, edit here ===

import os, subprocess, time, hashlib, shutil
from datetime import datetime
import requests
from .config import *
from . import state
from .helpers import *


# V4.52: OTA server removed. The GUI POSTs the .bin directly to the
# bike's /ota endpoint. No local HTTP server needed.

