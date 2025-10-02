$(document).ready(function () {
	let lastTs = null;
	let allMessages = [];
	let messageQueue = [];
	let sending = false;

	function receiveMessages() {
		let url = '/messages';
		if (lastTs) url += '?since=' + encodeURIComponent(lastTs);

		$.get(url, function (response) {
			let messages = [];

			if (typeof response === 'string') {
				try
				{
					messages = JSON.parse(response);
				}
				catch (e)
				{
					console.log('messages parse error', e, response);
					messages = [];
				}
			}

			else if (Array.isArray(response)) messages = response;

			if (messages.length > 0) {
				// Update lastTs to the newest message
				lastTs = messages[messages.length - 1].ts;

				// Only add truly new messages
				let newCount = 0;
				messages.forEach(function (msg) {

					// Check if this message is already in allMessages
					if (!allMessages.some(m => m.ts === msg.ts && m.user === msg.user && m.text === msg.text)) {
						allMessages.push(msg);
						newCount++;
					}
				});

				// Only re-render if new messages were added
				if (newCount > 0) {
					const $chatMessages = $('#chat-messages');
					$chatMessages.empty();
					allMessages.forEach(function (msg) {
						// Format timestamp (assuming ts is a UNIX timestamp in milliseconds)
						let date = new Date(msg.ts);
						let timeString = formatDateTime(date);
						let messageText = `<span class="chat-timestamp">[${timeString}]</span> <strong>${msg.user}:</strong> ${msg.text}`;
						const $msgDiv = $('<div>').addClass('chat-message').html(messageText);
						$chatMessages.append($msgDiv);
					});

					if ($chatMessages[0]) $chatMessages.scrollTop($chatMessages[0].scrollHeight);
				}
			}
		}, 'json');
	}

	// Poll for new messages every 2 seconds
	setInterval(receiveMessages, 2000);
	receiveMessages();

	function getCookie(name) {
		let match = document.cookie.match(new RegExp('(^| )' + name + '=([^;]+)'));
		return match ? match[2] : '';
	}

	function sendNextMessage() {
		if (sending || messageQueue.length === 0) return;
		sending = true;
		const { text, input } = messageQueue.shift();

		fetch('https://api.ipify.org?format=json')
			.then(res => res.json())
			.then(data => {
				const device_id = getCookie('device_id');
				$.ajax({
					url: '/messages',
					type: 'POST',
					contentType: 'application/json',
					data: JSON.stringify({ text: text, ip: data.ip, device_id: device_id }),
					success: function () {
						input.val('');
						lastTs = null;
						allMessages = [];
						setTimeout(receiveMessages, 100);
					},
					complete: function () {
						sending = false;
						sendNextMessage(); // Send next message in queue
					}
				});
			});
	}

	$('.chat-form').on('submit', function (e) {
		e.preventDefault();
		const input = $(this).find('.chat-input');
		const text = input.val().trim();
		if (text) {
			messageQueue.push({ text, input });
			sendNextMessage();
		}
	});

	$('#change-username-btn').on('click', function () {
		window.location.href = '/username';
	});

	function formatDateTime(date) {
		const now = new Date();
		const day = String(date.getDate()).padStart(2, '0');
		const month = String(date.getMonth() + 1).padStart(2, '0');
		const year = date.getFullYear();
		const hours = String(date.getHours()).padStart(2, '0');
		const minutes = String(date.getMinutes()).padStart(2, '0');

		let datePart = '';
		
		if (year !== now.getFullYear()) datePart = `${day}/${month}/${year}`;
		
		else if (day !== String(now.getDate()).padStart(2, '0') || month !== String(now.getMonth() + 1).padStart(2, '0')) datePart = `${day}/${month}`;
		
		return datePart ? `${datePart} ${hours}:${minutes}` : `${hours}:${minutes}`;
	}
});