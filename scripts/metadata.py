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

import utils
from generate_metadata import write_metadata

Import("env")

keyboard = utils.get_keyboard_name(env)


def project_bool_option(name: str) -> bool:
    try:
        value = env.GetProjectOption(name)
    except Exception:
        return False

    return str(value).lower() not in ("", "0", "false", "no")


write_metadata(
    keyboard,
    diag_channel_identity=project_bool_option("custom_diag_channel_identity"),
    diag_runtime=project_bool_option("custom_diag_runtime"),
)
