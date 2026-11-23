const canvas = document.getElementById("campus-map");
const ctx = canvas.getContext("2d");

const west = -83.9474800;
const east = -83.9198400;
const south = 35.9423700;
const north = 35.9614000;

fetch("../data/map.txt")
    .then(response => {
        if (!response.ok) {
            throw new Error(`Could not load map.txt (HTTP ${response.status})`);
        }

        return response.text();
    })
    .then(text => {
        const lines = text.split("\n");
        const nodesById = {};

        // Draw each node and save its canvas coordinates by ID.
        for (const line of lines) {
            if (!line.startsWith("NODE ")) {
                continue;
            }

            const fields = line.split(" ");
            const id = fields[1];
            const lat = Number(fields[2]);
            const lon = Number(fields[3]);

            const x = (lon - west) / (east - west) * canvas.width;
            const y = (north - lat) / (north - south) * canvas.height;

            nodesById[id] = { x: x, y: y };

            ctx.beginPath();
            ctx.arc(x, y, 1, 0, 2 * Math.PI);
            ctx.fill();
        }

        // Connect the two nodes listed in each edge record.
        ctx.beginPath();

        for (const line of lines) {
            if (!line.startsWith("EDGE ")) {
                continue;
            }

            const fields = line.split(" ");
            const from = nodesById[fields[1]];
            const to = nodesById[fields[2]];

            if (!from || !to) {
                continue;
            }

            ctx.moveTo(from.x, from.y);
            ctx.lineTo(to.x, to.y);
        }

        ctx.stroke();

        // Collect building coordinates and full names.
        const buildings = [];

        for (const line of lines) {
            if (!line.startsWith("BUILDING ")) {
                continue;
            }

            const fields = line.split(" ");

            buildings.push({
                lat: Number(fields[1]),
                lon: Number(fields[2]),
                name: fields.slice(3).join(" ")
            });
        }

        // Draw each building dot and save its position for hover.
        for (const building of buildings) {
            // Comment out this line to hide the full list in the Console.
            console.log(building.name);

            building.x = (building.lon - west) / (east - west) * canvas.width;
            building.y = (north - building.lat) / (north - south) * canvas.height;

            ctx.beginPath();
            ctx.arc(building.x, building.y, 3, 0, 2 * Math.PI);
            ctx.fill();
        }

        // Create a paragraph immediately below the canvas.
        const hoverName = document.createElement("p");
        hoverName.textContent = "Hover over a building marker.";
        canvas.after(hoverName);

        canvas.addEventListener("pointermove", event => {
            const rect = canvas.getBoundingClientRect();

            // Convert the pointer position to canvas coordinates.
            const mouseX = (event.clientX - rect.left - canvas.clientLeft)
                * canvas.width / canvas.clientWidth;
            const mouseY = (event.clientY - rect.top - canvas.clientTop)
                * canvas.height / canvas.clientHeight;

            let closestBuilding = null;
            let closestDistance = 8;

            for (const building of buildings) {
                const distance = Math.hypot(
                    mouseX - building.x,
                    mouseY - building.y
                );

                if (distance < closestDistance) {
                    closestBuilding = building;
                    closestDistance = distance;
                }
            }

            hoverName.textContent = closestBuilding
                ? closestBuilding.name
                : "Hover over a building marker.";
        });

        canvas.addEventListener("pointerleave", () => {
            hoverName.textContent = "Hover over a building marker.";
        });
    })
    .catch(error => {
        console.error(error);
    });