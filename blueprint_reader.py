import re

def parse_node_block(block):
    """
    Parse a single blueprint node block and extract details.
    Returns a dictionary with keys:
      - name: Node name
      - node_type: The type/class of the node (e.g. K2Node_CallFunction)
      - pos: (NodePosX, NodePosY)
      - function_reference: If the node calls a function (MemberName)
      - variable_reference: If the node accesses a variable (MemberName)
      - pins: List of dictionaries with details for each pin
    """
    details = {}
    # The header is the first line in the block.
    header_line = block.splitlines()[0].strip()

    # Extract node type (the class after "Class=")
    class_match = re.search(r'Class=([^ ]+)', header_line)
    details['node_type'] = class_match.group(1) if class_match else "UnknownType"

    # Extract node name (the value after Name="...")
    name_match = re.search(r'Name="([^"]+)"', header_line)
    details['name'] = name_match.group(1) if name_match else "Unnamed"

    # Extract node position X and Y
    posx_match = re.search(r'NodePosX=([\d-]+)', block)
    posy_match = re.search(r'NodePosY=([\d-]+)', block)
    pos_x = posx_match.group(1) if posx_match else "N/A"
    pos_y = posy_match.group(1) if posy_match else "N/A"
    details['pos'] = (pos_x, pos_y)

    # Extract function reference (if any)
    func_match = re.search(r'FunctionReference=\(MemberName="([^"]+)"', block)
    details['function_reference'] = func_match.group(1) if func_match else None

    # Extract variable reference (if any)
    var_match = re.search(r'VariableReference=\(MemberName="([^"]+)"', block)
    details['variable_reference'] = var_match.group(1) if var_match else None

    # Extract pin details. We search for each "CustomProperties Pin (...)" block.
    pins = []
    # This pattern grabs everything between "CustomProperties Pin (" and the closing ")"
    pin_pattern = re.compile(r'CustomProperties Pin \((.*?)\)', re.DOTALL)
    for pin_block in pin_pattern.findall(block):
        pin_info = {}
        # Extract the pin's name.
        pin_name_match = re.search(r'PinName="([^"]+)"', pin_block)
        pin_info['pin_name'] = pin_name_match.group(1) if pin_name_match else "UnnamedPin"
        # Extract the pin type category.
        pin_category_match = re.search(r'PinType\.PinCategory="([^"]+)"', pin_block)
        pin_info['category'] = pin_category_match.group(1) if pin_category_match else "UnknownCategory"
        # Try to extract the pin's direction (if available)
        direction_match = re.search(r'Direction="([^"]+)"', pin_block)
        pin_info['direction'] = direction_match.group(1) if direction_match else "Default"
        pins.append(pin_info)
    details['pins'] = pins

    return details

def generate_human_narrative(details):
    """
    Given the extracted node details, generate a human-level narrative string.
    """
    narrative = f"Node '{details['name']}' is a {details['node_type']} located at position {details['pos']}.\n"

    # Describe what the node does based on its type and references.
    if details['function_reference']:
        narrative += f"  It calls the function '{details['function_reference']}'.\n"
    if details['variable_reference']:
        narrative += f"  It accesses the variable '{details['variable_reference']}'.\n"

    # If any pins were found, list some details.
    if details['pins']:
        narrative += f"  It has {len(details['pins'])} pin(s):\n"
        for pin in details['pins']:
            narrative += f"    - Pin '{pin['pin_name']}' of type '{pin['category']}' (Direction: {pin['direction']}).\n"
    return narrative

def main():
    try:
        with open("blueprint.txt", "r", encoding="utf-8") as f:
            blueprint_text = f.read()
    except Exception as e:
        print("Error reading blueprint.txt:", e)
        return

    # Split the text into node blocks using "Begin Object" as a delimiter.
    # Only consider blocks that contain "End Object".
    blocks = [block for block in re.split(r'Begin Object', blueprint_text) if "End Object" in block]

    narrative_list = []
    for block in blocks:
        details = parse_node_block(block)
        narrative = generate_human_narrative(details)
        narrative_list.append(narrative)

    full_narrative = "\n".join(narrative_list)
    print("Blueprint Narrative:\n")
    print(full_narrative)

if __name__ == "__main__":
    main()
