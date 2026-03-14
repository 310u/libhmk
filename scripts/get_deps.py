# This program is free software: you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option) any later
# version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License along with
# this program. If not, see <https://www.gnu.org/licenses/>.

import importlib.util

Import("env")

REQUIRED_MODULES = {
    "jsonschema": "jsonschema",
}

for module_name, package_name in REQUIRED_MODULES.items():
    if importlib.util.find_spec(module_name) is not None:
        continue

    result = env.Execute(f"$PYTHONEXE -m pip install {package_name} --upgrade")
    if result != 0:
        print(f"Failed to install required Python package: {package_name}")
        Exit(1)
