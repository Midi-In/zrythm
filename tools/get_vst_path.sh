#! /bin/sh

#  Copyright (C) 2020 Alexandros Theodotou <alex at zrythm dot org>
#
#  This file is part of Zrythm
#
#  Zrythm is free software: you can redistribute it and/or modify
#  it under the terms of the GNU Affero General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  Zrythm is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU Affero General Public License for more details.
#
#  You should have received a copy of the GNU Affero General Public License
#  along with Zrythm.  If not, see <https://www.gnu.org/licenses/>.
#

# This is used during the tests to create a temporary
# lv2 plugin environment before calling lv2lint

set -e

plugin_filename=$1

for i in $(echo $VST_PATH | sed "s/:/ /g"); do
  full_path="$i/$plugin_filename"
  if ls $full_path; then
    exit 0
  fi
done

exit 1
